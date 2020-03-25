#pragma once

#if HAS_CXX11_THREAD_LOCAL
#define CTX_ATTRIBUTE_TLS thread_local
#elif defined(__GNUC__)
#define CTX_ATTRIBUTE_TLS __thread
#elif defined(_MSC_VER)
#define CTX_ATTRIBUTE_TLS __declspec(thread)
#else  // !C++11 && !__GNUC__ && !_MSC_VER
#error "Define a thread local storage qualifier for your compiler/platform!"
#endif