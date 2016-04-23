#define BMH_CHARSET_SIZE 10
#define BMH_PATLEN_MAX 9
#define BMH_BAD_PATTERN (-2)

int js_BoyerMooreHorspool31(const char *text, int textlen,
			    const char *pat, int patlen,
			    int start);

int js_BoyerMooreHorspool43(const char *text, int textlen,
			    const char *pat, int patlen,
			    int start);

int js_BoyerMooreHorspool(const char *text, int textlen,
			  const char *pat, int patlen,
			  int start);
