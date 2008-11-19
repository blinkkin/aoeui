/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

Boolean_t no_keywords;

static const char *C_keyword_list[] = {
	"#define", "#elif", "#else", "#endif", "#if", "#ifdef", "#ifndef",
	"#include", "#pragma", "#undef",
	"asm", "auto", "break", "case", "char", "const", "continue",
	"default", "do", "double", "else", "enum", "extern", "float",
	"for", "goto", "if", "int", "long", "register", "return",
	"short", "signed", "sizeof", "static", "struct", "switch",
	"typedef", "union", "unsigned", "void", "volatile", "while"
};

static const char *Cpp_keyword_list[] = {
	"#define", "#elif", "#else", "#endif", "#if", "#ifdef", "#ifndef",
	"#include", "#pragma", "#undef",
	"asm", "auto", "bool", "break", "case", "catch", "char", "class",
	"const", "const_cast", "continue", "default", "delete", "do", "double",
	"dynamic_cast", "else", "enum", "explicit", "export", "extern", "false",
	"float", "for", "friend", "goto", "if", "inline", "int", "long",
	"mutable", "namespace", "new", "operator", "private", "protected",
	"public", "register", "reinterpret_cast", "return", "short", "signed",
	"sizeof", "static", "static_cast", "struct", "switch", "template",
	"this", "throw", "true", "try", "typedef", "typeid", "typename",
	"union", "unsigned", "using", "virtual", "void", "volatile",
	"wchar_t", "while"
};

#define KW(lang) { sizeof lang##_keyword_list / sizeof *lang##_keyword_list, \
		   lang##_keyword_list }

static struct file_keywords {
	const char *suffix;
	struct keywords keywords;
	const char *brackets;
} kwmap [] = {
	{ ".c", KW(C), "()[]{}" },
	{ ".C", KW(C), "()[]{}" },
	{ ".cc", KW(Cpp), "()[]{}" },
	{ ".cpp", KW(Cpp), "()[]{}" },
	{ ".cxx", KW(Cpp), "()[]{}" },
	{ ".h", KW(Cpp), "()[]{}" },
	{ ".html", { 0, NULL }, "<>" },
	{ }
};

void keyword_init(struct text *text)
{
	if (text->path) {
		size_t pathlen = strlen(text->path);
		int j;
		for (j = 0; kwmap[j].suffix; j++) {
			size_t suffixlen = strlen(kwmap[j].suffix);
			if (pathlen > suffixlen &&
			    !strcmp(text->path + pathlen - suffixlen,
				    kwmap[j].suffix)) {
				text->brackets = kwmap[j].brackets;
				if (!no_keywords)
					text->keywords = &kwmap[j].keywords;
				return;
			}
		}
	}
	text->brackets = "()[]{}";
	text->keywords = NULL;
}

Boolean_t is_keyword(struct view *view, position_t offset)
{
	unsigned n;
	const char **tab;
	char *word;
	size_t bytes;

	if (!view->text->keywords)
		return FALSE;
	bytes = find_id_end(view, offset + 1) - offset;
	if (view_raw(view, &word, offset, bytes) < bytes)
		return FALSE;
	for (n = view->text->keywords->count,
	     tab = view->text->keywords->word; n; ) {
		unsigned mid = n / 2;
		int cmp = strncmp(word, tab[mid], bytes);
		if (!cmp)
			if (tab[mid][bytes])
				cmp = -1;
			else
				return TRUE;
		if (cmp < 0)
			n = mid;
		else {
			n -= mid + 1;
			tab += mid + 1;
		}
	}
	return FALSE;
}
