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

static const char *Haskell_keyword_list[] = {
	"case", "class", "data", "default", "deriving", "do", "else",
	"foreign", "if", "import", "in", "infix", "infixl", "infixr",
	"instance", "let", "module", "newtype", "of", "then", "type",
	"where", "_"
};

static sposition_t C_comment_start(struct view *view, position_t offset)
{
	Unicode_t ch, nch = 0;
	int newlines = 0;

	while (IS_UNICODE((ch = view_char_prior(view, offset, &offset)))) {
		if (ch == '\n') {
			if (newlines++ == 100)
				break;
		} else if (ch == '/') {
			if (nch == '*')
				return offset;
			if (!newlines && nch == '/')
				return offset;
		} else if (ch == '*' && nch == '/')
			break;
		nch = ch;
	}
	return -1;
}

static sposition_t C_comment_end(struct view *view, position_t offset)
{
	Unicode_t ch, lch = 0;
	position_t next;

	if (view_char(view, offset, &offset) != '/')
		return -1;
	ch = view_char(view, offset, &offset);
	if (ch == '/')
		return find_line_end(view, offset);
	if (ch != '*')
		return -1;
	while (IS_UNICODE((ch = view_char(view, offset, &next)))) {
		if (lch == '*' && ch == '/')
			return offset;
		lch = ch;
		offset = next;
	}
	return -1;
}

static sposition_t C_string_end(struct view *view, position_t offset)
{
	Unicode_t ch, lch = 0, ch0 = view_char(view, offset, &offset);
	position_t next;

	if (ch0 != '\'' && ch0 != '"')
		return -1;
	while (IS_UNICODE((ch = view_char(view, offset, &next)))) {
		if (ch == ch0 && lch != '\\')
			return offset;
		if (ch == '\n')
			break;
		if (ch == '\\' && lch == '\\')
			lch = 0;
		else
			lch = ch;
		offset = next;
	}
	return -1;
}

static sposition_t Haskell_comment_start(struct view *view, position_t offset)
{
	Unicode_t ch, nch = 0;
	int newlines = 0;

	while (IS_UNICODE((ch = view_char_prior(view, offset, &offset)))) {
		if (ch == '\n') {
			if (newlines++ == 100)
				break;
		} else if (ch == '{' && nch == '-')
			return offset;
		else if (ch == '-') {
			if (!newlines && nch == '-')
				return offset;
			if (nch == '}')
				break;
		}
		nch = ch;
	}
	return -1;
}

static sposition_t Haskell_comment_end(struct view *view, position_t offset)
{
	Unicode_t ch, lch = 0;
	position_t next;

	ch = view_char(view, offset, &offset);
	if (ch != '-' && ch != '{')
		return -1;
	if (view_char(view, offset, &offset) != '-')
		return -1;
	if (ch == '-')
		return find_line_end(view, offset);
	while (IS_UNICODE((ch = view_char(view, offset, &next)))) {
		if (lch == '-' && ch == '}')
			return offset;
		lch = ch;
		offset = next;
	}
	return -1;
}

static sposition_t Haskell_string_end(struct view *view, position_t offset)
{
	position_t next;
	Unicode_t ch, lch = 0, ch0 = view_char(view, offset, &next);

	if (ch0 == '\'') {
		ch = view_char_prior(view, offset, NULL);
		if (isalnum(ch) || ch == '_' || ch == '\'')
			return -1;
	} else if (ch0 != '"')
		return -1;
	while (IS_UNICODE((ch = view_char(view, offset = next, &next)))) {
		if (ch == ch0 && lch != '\\')
			return offset;
		if (ch == '\n')
			break;
		if (ch == '\\' && lch == '\\')
			lch = 0;
		else
			lch = ch;
	}
	return -1;
}

#define KW(lang) { sizeof lang##_keyword_list / sizeof *lang##_keyword_list, \
		   lang##_keyword_list }

static struct file_keywords {
	const char *suffix;
	struct keywords keywords;
	const char *brackets;
	sposition_t (*comment_start)(struct view *, position_t);
	sposition_t (*comment_end)(struct view *, position_t);
	sposition_t (*string_end)(struct view *, position_t);
} kwmap [] = {
	{ ".c", KW(C), "()[]{}", C_comment_start, C_comment_end, C_string_end },
	{ ".C", KW(C), "()[]{}", C_comment_start, C_comment_end, C_string_end },
	{ ".cc", KW(Cpp), "()[]{}", C_comment_start, C_comment_end, C_string_end },
	{ ".cpp", KW(Cpp), "()[]{}", C_comment_start, C_comment_end, C_string_end },
	{ ".cxx", KW(Cpp), "()[]{}", C_comment_start, C_comment_end, C_string_end },
	{ ".h", KW(Cpp), "()[]{}", C_comment_start, C_comment_end, C_string_end },
	{ ".hs", KW(Haskell), "()[]{}", Haskell_comment_start, Haskell_comment_end,
					Haskell_string_end },
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
				if (!no_keywords) {
					text->keywords = &kwmap[j].keywords;
					text->comment_start = kwmap[j].comment_start;
					text->comment_end = kwmap[j].comment_end;
					text->string_end = kwmap[j].string_end;
				}
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
