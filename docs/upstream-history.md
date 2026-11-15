# Upstream project history

The following account is by the author of magiblot/tvision. Dates and goals
refer to that upstream project, not to TVisioAllIn. DOS/Borland is not a
supported build target of this distribution.

I started this as a personal project at the very end of 2018. By May 2020 I considered it was very close to feature parity with the original, and decided to make it open.

The original goals of this project were:

* Making Turbo Vision work on Unix/Linux by altering the legacy codebase as little as possible.
* Keeping it functional on DOS/Windows.
* Being as compatible as possible at the source code level with old Turbo Vision applications. This led me to implement some of the Borland C++ RTL functions, as explained below.

At one point I considered I had done enough, and that any attempts at revamping the library and overcoming its original limitations would require either extending the API or breaking backward compatibility, and that a major rewrite would be most likely necessary.

However, between July and August 2020 I found the way to integrate full-fledged Unicode support into the existing architecture, wrote the [Turbo](https://github.com/magiblot/turbo) text editor and also made the new features available on Windows. So I am confident that Turbo Vision can now meet many of the expectations of modern users and programmers.

The original location of this project is https://github.com/magiblot/tvision.
