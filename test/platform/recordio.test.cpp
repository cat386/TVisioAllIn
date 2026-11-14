#include <internal/recordio.h>
#include <test.h>
#include <string>
TEST(RecordReader, KeepsFragmentsAcrossWouldBlockAndRetriesInterrupts) {
    tvision::RecordReader<4> reader;char out[4]={};int step=0;
    auto receive=[&](void *dst,size_t size)->ptrdiff_t {
        switch(step++) {
        case 0: errno=EINTR;return -1;
        case 1: EXPECT_EQ(size,4u);memcpy(dst,"ab",2);return 2;
        case 2: errno=EAGAIN;return -1;
        case 3: EXPECT_EQ(size,2u);memcpy(dst,"c",1);return 1;
        default: EXPECT_EQ(size,1u);memcpy(dst,"d",1);return 1;
        }
    };
    EXPECT_FALSE(reader.read(out,receive));EXPECT_EQ(out[0],0);
    EXPECT_TRUE(reader.read(out,receive));EXPECT_EQ(std::string(out,4),"abcd");
}
TEST(RecordReader, SeparatesRecordsAndResetsAfterEof) {
    tvision::RecordReader<4> reader;char out[4]={};int step=0;
    auto receive=[&](void *dst,size_t)->ptrdiff_t {
        switch(step++) {
        case 0:memcpy(dst,"xx",2);return 2;
        case 1:return 0;
        default:memcpy(dst,"next",4);return 4;
        }
    };
    EXPECT_FALSE(reader.read(out,receive));EXPECT_TRUE(reader.read(out,receive));
    EXPECT_EQ(std::string(out,4),"next");
    EXPECT_TRUE(reader.read(out,receive));EXPECT_EQ(std::string(out,4),"next");
}

#ifndef _WIN32
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
TEST(RecordReader, ReceivesFragmentedUnixSocketRecords) {
    int fd[2];ASSERT_EQ(socketpair(AF_UNIX,SOCK_STREAM,0,fd),0);
    ASSERT_NE(fcntl(fd[1],F_SETFL,O_NONBLOCK),-1);
    tvision::RecordReader<28> reader;char out[28]={};
    auto receive=[&](void *data,size_t size) { return ::read(fd[1],data,size); };
    std::string first(28,'a'),second(28,'b');
    ASSERT_EQ(write(fd[0],first.data(),7),7);
    EXPECT_FALSE(reader.read(out,receive));
    std::string tail=first.substr(7)+second;
    ASSERT_EQ(write(fd[0],tail.data(),tail.size()),(ssize_t)tail.size());
    ASSERT_TRUE(reader.read(out,receive));EXPECT_EQ(std::string(out,28),first);
    ASSERT_TRUE(reader.read(out,receive));EXPECT_EQ(std::string(out,28),second);
    close(fd[0]);close(fd[1]);
}
#endif
