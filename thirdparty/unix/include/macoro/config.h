

#define MACORO_CPP_20 ON
/* #undef MACORO_VARIANT_LITE_V */
/* #undef MACORO_OPTIONAL_LITE_V */
#define MACORO_HAS_STD_STOP_TOKEN ON


#ifdef MACORO_CPP_20
#define MACORO_NODISCARD [[nodiscard]]
#define MACORO_FALLTHROUGH [[fallthrough]]
#define MACORO_OPERATOR_COAWAIT operator co_await
#else
#define MACORO_NODISCARD
#define MACORO_FALLTHROUGH
#define MACORO_OPERATOR_COAWAIT operator_co_await
#endif




#define MACORO_STRINGIZE_DETAIL(x) #x
#define MACORO_STRINGIZE(x) MACORO_STRINGIZE_DETAIL(x)
#define MACORO_LOCATION __FILE__ ":" MACORO_STRINGIZE(__LINE__)
#define MACORO_RTE_LOC std::runtime_error(MACORO_LOCATION)
#define MACORO_ASSUME(x)


#if _MSC_VER
# define MACORO_WINDOWS_OS 1
# define MACORO_NOINLINE __declspec(noinline)
#else
# define MACORO_WINDOWS_OS 0
# define MACORO_NOINLINE __attribute__((noinline))
#endif
# define MACORO_CPU_CACHE_LINE 64

