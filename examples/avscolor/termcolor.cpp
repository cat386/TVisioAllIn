// TermColor: AviSynth filter that demonstrates the terminal color
// quantizations used by Turbo Vision.
//
// Tested with AVS+ 3.7, Linux x86_64.
//
// TermColor(clip c, string "mode")
//
// * clip c: input clip must be RGB24 or RGB32.
// * string mode: one of: indexed8, indexed16, indexed256, direct.
//
// This plugin uses AviSynth's C plugin interface, whose binary interface is
// stable and simple enough to be declared here (see 'namespace avs' below).
// The AviSynth functions it calls are looked up at runtime in the AviSynth
// library already loaded by the host process, so neither the AviSynth headers
// nor the AviSynth library are needed to build it.

#define Uses_TPoint
#define Uses_TColorAttr
#include <tvision/tv.h>

#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#else
#include <link.h>
#endif

/////////////////////////////////////////////////////////////////////////
// AviSynth C interface (binary-compatible subset of 'avisynth_c.h').

namespace avs
{

struct ScriptEnvironment;
struct Clip;
struct VideoFrame;

struct VideoInfo
{
    int width, height;
    unsigned fps_numerator, fps_denominator;
    int num_frames;
    int pixel_type;
    int audio_samples_per_second;
    int sample_type;
    int64_t num_audio_samples;
    int nchannels;
    int image_type;
};

struct Value
{
    short type; // 'a'rray, 'c'lip, 'b'ool, 'i'nt, 'f'loat, 's'tring, 'v'oid, 'e'rror...
    short array_size;
    union
    {
        void *clip;
        char boolean;
        int integer;
        float floating_pt;
        const char *string;
        const Value *array;
        void *function;
        int64_t longlong;
        double double_pt;
    } d;
};

struct FilterInfo
{
    Clip *child;
    VideoInfo vi;
    ScriptEnvironment *env;
    VideoFrame *(*get_frame)(FilterInfo *, int n);
    int (*get_parity)(FilterInfo *, int n);
    int (*get_audio)(FilterInfo *, void *buf, int64_t start, int64_t count);
    int (*set_cache_hints)(FilterInfo *, int cachehints, int frame_range);
    void (*free_filter)(FilterInfo *);
    const char *error;
    void *user_data;
};

using ApplyFunc = Value (*)(ScriptEnvironment *, Value args, void *user_data);

enum
{
    CS_BGR = 1 << 28,
    CS_INTERLEAVED = 1 << 30,
    CS_RGB_TYPE = 1 << 0,
    CS_RGBA_TYPE = 1 << 1,
    CS_SAMPLE_BITS_MASK = 7 << 16,
    CS_SAMPLE_BITS_8 = 0,
    CS_BGR24 = CS_RGB_TYPE | CS_BGR | CS_INTERLEAVED,
    CS_BGR32 = CS_RGBA_TYPE | CS_BGR | CS_INTERLEAVED,

    DEFAULT_PLANE = 0,

    CACHE_GET_MTMODE = 509,
    MT_NICE_FILTER = 1,
};

inline bool isRGB24(const VideoInfo &vi)
{
    return (vi.pixel_type & CS_BGR24) == CS_BGR24 &&
           (vi.pixel_type & CS_SAMPLE_BITS_MASK) == CS_SAMPLE_BITS_8;
}

inline bool isRGB32(const VideoInfo &vi)
{
    return (vi.pixel_type & CS_BGR32) == CS_BGR32 &&
           (vi.pixel_type & CS_SAMPLE_BITS_MASK) == CS_SAMPLE_BITS_8;
}

inline Value arrayElt(Value v, int index)
{
    return v.type == 'a' ? v.d.array[index] : v;
}

inline Value newError(const char *message)
{
    Value v {};
    v.type = 'e';
    v.d.string = message;
    return v;
}

// Functions exported by the AviSynth library.
struct Api
{
    int (*add_function)(ScriptEnvironment *, const char *name, const char *params, ApplyFunc apply, void *user_data);
    Clip *(*new_c_filter)(ScriptEnvironment *, FilterInfo **fi, Value child, int store_child);
    void (*set_to_clip)(Value *, Clip *);
    void (*release_clip)(Clip *);
    VideoFrame *(*get_frame)(Clip *, int n);
    int (*make_writable)(ScriptEnvironment *, VideoFrame **pvf);
    int (*get_pitch_p)(const VideoFrame *, int plane);
    const uint8_t *(*get_read_ptr_p)(const VideoFrame *, int plane);
    uint8_t *(*get_write_ptr_p)(const VideoFrame *, int plane);

    bool ok {false};
};

static Api api;

#ifndef __APPLE__
static int findAvisynthLibrary(struct dl_phdr_info *info, size_t, void *data)
{
    if (info->dlpi_name && strstr(info->dlpi_name, "libavisynth"))
    {
        *(void **) data = dlopen(info->dlpi_name, RTLD_NOW | RTLD_NOLOAD);
        if (*(void **) data)
            return 1;
    }
    return 0;
}
#endif

static void *openAvisynthLibrary()
{
    // The plugin is always loaded by AviSynth, so its library is already
    // in the process.
    void *handle = nullptr;
#ifdef __APPLE__
    for (uint32_t i = 0; i < _dyld_image_count() && !handle; ++i)
    {
        const char *name = _dyld_get_image_name(i);
        if (name && strstr(name, "libavisynth"))
            handle = dlopen(name, RTLD_NOW | RTLD_NOLOAD);
    }
#else
    dl_iterate_phdr(findAvisynthLibrary, &handle);
#endif
    return handle;
}

template <class T>
static bool loadSymbol(void *handle, T &fn, const char *name)
{
    // Prefer symbols already visible globally; otherwise, look them up in
    // the AviSynth library (which may have been loaded with RTLD_LOCAL).
    void *sym = dlsym(RTLD_DEFAULT, name);
    if (!sym && handle)
        sym = dlsym(handle, name);
    fn = (T) sym;
    return sym != nullptr;
}

static bool loadApi()
{
    if (!api.ok)
    {
        void *handle = openAvisynthLibrary();
        api.ok =
            loadSymbol(handle, api.add_function, "avs_add_function") &&
            loadSymbol(handle, api.new_c_filter, "avs_new_c_filter") &&
            loadSymbol(handle, api.set_to_clip, "avs_set_to_clip") &&
            loadSymbol(handle, api.release_clip, "avs_release_clip") &&
            loadSymbol(handle, api.get_frame, "avs_get_frame") &&
            loadSymbol(handle, api.make_writable, "avs_make_writable") &&
            loadSymbol(handle, api.get_pitch_p, "avs_get_pitch_p") &&
            loadSymbol(handle, api.get_read_ptr_p, "avs_get_read_ptr_p") &&
            loadSymbol(handle, api.get_write_ptr_p, "avs_get_write_ptr_p");
    }
    return api.ok;
}

} // namespace avs

/////////////////////////////////////////////////////////////////////////
// The filter.

enum TermColors
{
    Indexed8,
    Indexed16,
    Indexed256,
    Direct,
    TermColorCount,
};

enum PixelTypes
{
    RGB24,
    RGB32,
    PixelTypeCount,
};

using FrameProcessor = void (const avs::VideoFrame *, avs::VideoFrame *, TPoint);

template <uint32_t transform(uint32_t, TPoint), bool alpha>
static void process_rgb(const avs::VideoFrame *src, avs::VideoFrame *dst, TPoint size)
{
    const uint8_t *srcp = avs::api.get_read_ptr_p(src, avs::DEFAULT_PLANE);
    uint8_t *dstp = avs::api.get_write_ptr_p(dst, avs::DEFAULT_PLANE);
    auto src_pitch = avs::api.get_pitch_p(src, avs::DEFAULT_PLANE),
         dst_pitch = avs::api.get_pitch_p(dst, avs::DEFAULT_PLANE);
    constexpr size_t pixel_size = 3 + alpha;
    for (int y = 0; y < size.y; ++y)
    {
        size_t stride = 0;
        for (int x = 0; x < size.x; ++x, stride += pixel_size)
        {
            uint32_t in = 0;
            memcpy(&in, &srcp[stride], pixel_size);
            uint32_t out = transform(in, {x, y});
            if (alpha) out |= in & 0xFF000000;
            memcpy(&dstp[stride], &out, pixel_size);
        }
        srcp += src_pitch;
        dstp += dst_pitch;
    }
}

static inline uint32_t quantize_indexed8(uint32_t, TPoint);
static inline uint32_t quantize_indexed16(uint32_t, TPoint);
static inline uint32_t quantize_indexed256(uint32_t, TPoint);

static constexpr FrameProcessor *frameProcessors[PixelTypeCount][TermColorCount] =
{
    { // RGB24
        &process_rgb<quantize_indexed8, false>,
        &process_rgb<quantize_indexed16, false>,
        &process_rgb<quantize_indexed256, false>,
        nullptr,
    },
    { // RGB32
        &process_rgb<quantize_indexed8, true>,
        &process_rgb<quantize_indexed16, true>,
        &process_rgb<quantize_indexed256, true>,
        nullptr,
    },
};

static constexpr struct { const char *name; TermColors value; } quantizeModes[] =
{
    {"indexed8", Indexed8},
    {"indexed16", Indexed16},
    {"indexed256", Indexed256},
    {"direct", Direct},
};

static avs::VideoFrame *getFrame(avs::FilterInfo *fi, int n)
{
    avs::VideoFrame *frame = avs::api.get_frame(fi->child, n);
    auto *processor = (FrameProcessor *) fi->user_data;
    if (frame && processor)
    {
        avs::api.make_writable(fi->env, &frame);
        (*processor)(frame, frame, {fi->vi.width, fi->vi.height});
    }
    return frame;
}

static int setCacheHints(avs::FilterInfo *, int cachehints, int)
{
    return cachehints == avs::CACHE_GET_MTMODE ? avs::MT_NICE_FILTER : 0;
}

static avs::Value create(avs::ScriptEnvironment *env, avs::Value args, void *)
{
    avs::Value child = avs::arrayElt(args, 0);
    avs::Value modeArg = avs::arrayElt(args, 1);

    avs::FilterInfo *fi;
    avs::Clip *clip = avs::api.new_c_filter(env, &fi, child, 1);

    PixelTypes pixel = PixelTypeCount;
    if (avs::isRGB24(fi->vi))
        pixel = RGB24;
    else if (avs::isRGB32(fi->vi))
        pixel = RGB32;

    TermColors mode = TermColorCount;
    TStringView modeName = modeArg.type == 's' ? modeArg.d.string : nullptr;
    for (auto m : quantizeModes)
        if (m.name == modeName)
        {
            mode = m.value;
            break;
        }

    avs::Value result;
    if (pixel == PixelTypeCount)
        result = avs::newError("TermColor: input clip must be RGB24 or RGB32.");
    else if (mode == TermColorCount)
        result = avs::newError("TermColor: 'mode' must be one of: indexed8, indexed16, indexed256, direct.");
    else
    {
        fi->user_data = (void *) frameProcessors[pixel][mode];
        fi->get_frame = &getFrame;
        fi->set_cache_hints = &setCacheHints;
        avs::api.set_to_clip(&result, clip);
    }
    avs::api.release_clip(clip);
    return result;
}

extern "C" __attribute__((visibility("default")))
const char *avisynth_c_plugin_init(avs::ScriptEnvironment *env)
{
    if (!avs::loadApi())
        return "TermColor: cannot find the AviSynth library functions.";
    avs::api.add_function(env, "TermColor", "c[mode]s", &create, (void *) 1);
    return "TermColor: terminal color quantization used by Turbo Vision.";
}

static constexpr uint32_t xterm16_to_rgb[16] =
{
    0x000000,
    0x800000,
    0x008000,
    0x808000,
    0x000080,
    0x800080,
    0x008080,
    0xC0C0C0,
    0x808080,
    0xFF0000,
    0x00FF00,
    0xFFFF00,
    0x0000FF,
    0xFF00FF,
    0x00FFFF,
    0xFFFFFF,
};

static inline uint32_t quantize_indexed8(uint32_t rgb, TPoint pos)
{
    uint8_t idx = TColorConversion::RGBtoXTerm16(rgb);
    if (idx >= 8 && (pos.y % 2 != 0))
        idx -= 8;
    return xterm16_to_rgb[idx];
}

static inline uint32_t quantize_indexed16(uint32_t rgb, TPoint)
{
    uint8_t idx = TColorConversion::RGBtoXTerm16(rgb);
    return xterm16_to_rgb[idx];
}

static inline uint32_t quantize_indexed256(uint32_t rgb, TPoint)
{
    uint8_t idx = TColorConversion::RGBtoXTerm256(rgb);
    return TColorConversion::XTerm256toRGB(idx);
}
