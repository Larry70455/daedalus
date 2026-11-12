# Native code modules are generated C files (Tools/native_recomp). Every file in
# Generated/ is built and registers itself. Only modules for homebrew test ROMs
# are in the repository; modules made from commercial games are generated
# locally from the user's own ROM (see NATIVE_CODE.md) and ignored by git.
file(GLOB NATIVE_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/Generated/*.c)

add_library(Native OBJECT
                        NativeCode.cpp
                        ${NATIVE_MODULES}
                        )

list(APPEND daed_libs "Native")
set(daed_libs ${daed_libs} PARENT_SCOPE)
