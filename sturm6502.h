
/* addressing modes */
#define NONE    0x1    /* implied */
#define IMM     0x2    /* immediate */
#define ZP      0x4
#define ZP_X    0x8
#define ZP_Y    0x10
#define ABS     0x20
#define ABS_X   0x40
#define ABS_Y   0x80
#define REL     0x100  /* relative */
#define IND_X   0x200  /* (indirect,x) */
#define IND_Y   0x400  /* (indirect),y */
#define ABS_IND 0x800  /* (absolute indirect) */
#define ACC     0x1000 /* accumulator */
#define IMM2	0x2000 /* special handling for ldy, cpy, cpx, ldx */
#define ABS_Y2  0x4000 /* special handling for ldx */

/* misc defines */
#define MAX_LINE_LEN 128
#define EOL 0x0d
#define NOT_FOUND -1

/* tokens */
enum {
   TOKEN_EOL,
   TOKEN_CMD,
   TOKEN_COMMENT,
   TOKEN_A,
   TOKEN_X,
   TOKEN_Y,
   TOKEN_HASH,
   TOKEN_COMMA,
   TOKEN_LPAREN,
   TOKEN_RPAREN,
   TOKEN_MUL,
   TOKEN_LOW,
   TOKEN_HIGH,
   TOKEN_PLUS,
   TOKEN_MINUS,
   TOKEN_DIV,
   TOKEN_EQU,
   TOKEN_NUMBER,
   TOKEN_IDENT,
   TOKEN_STRING,
   TOKEN_DOT,
   TOKEN_PSEUDO, // 21
   TOKEN_CHAR,
   TOKEN_PARAM,
   TOKEN_LOCAL,
   TOKEN_UNKNOWN // 25
};
/* error messages */
enum {
   INVALID_MODE,
   SYNTAX_ERROR,
   INVALID_CMD,
   INVALID_EXPR,
   WRITE_ERROR,
   OPEN_ERROR,
   INVALID_BRANCH,
   READ_ERROR,
   INTERNAL_ERROR,
   INVALID_MACRO,
   OUT_OF_MEMORY,
   INVALID_DIV,
   INVALID_RPAREN,
   INVALID_LOCAL,
   INVALID_IDENT,
   INVALID_IF,
   INVALID_STRING,
   INVALID_PSEUDO
};

char *error_msg[] = {
   "Invalid addressing mode",
   "Syntax error",
   "Invalid command",
   "Invalid expression",
   "Write to file failed",
   "Cannot open file",
   "Branch out of range",
   "Cannot read file",
   "Internal error",
   "Macro already exists",
   "Out of memory",
   "Division by zero",
   "No right parenthesis",
   "No global label",
   "No identifier",
   "No .if command",
   "No string",
   "Invalid pseudo command"
};


char *delimiter_chars = "\t :";
char *comment_chars = ";*";

struct macro_param {
   char *str;
   struct macro_param *next;
};

struct macro_line {
   char *str;
   struct macro_line *next;
};

struct macro {
   char *name;
   struct macro_param *param;
   struct macro_line *line;
   struct symbol *local; /* local next symbol */
   struct macro *next;
};

struct instruction {
   char *mnemonic;
   unsigned int addr_modes;
   unsigned char op_code; /* base value */
};

struct token {
   unsigned int id;
   int value;
   char *label;
   struct instruction *inst;
   // struct macro *mac;
   struct token *next;
};

struct file {
   FILE *file;
   char *name;
   char *mode;
   unsigned int line_number;
};

struct symbol {
   unsigned int value;
   char *name;
   struct symbol *global; /* global next symbol */
   struct symbol *local; /* local next symbol */
};

struct instruction inst_a[] = {
   { "ADC", IMM|ZP|ZP_X|ABS|ABS_X|ABS_Y|IND_X|IND_Y, 0x61 },
   { "AND", IMM|ZP|ZP_X|ABS|ABS_X|ABS_Y|IND_X|IND_Y, 0x21 },
   { "ASL", ACC|ZP|ZP_X|ABS|ABS_X, 0x02 },
   { "", 0, 0}
};

struct instruction inst_b[] = {
   { "BCC", REL, 0x90 },
   { "BCS", REL, 0xb0 },
   { "BEQ", REL, 0xf0 },
   { "BIT", ZP|ABS, 0x20 },
   { "BMI", REL, 0x30 },
   { "BNE", REL, 0xd0 },
   { "BPL", REL, 0x10 },
   { "BRK", NONE, 0x00 },
   { "BVC", REL, 0x50 },
   { "BVS", REL, 0x70 },
   { "", 0, 0}
};

struct instruction inst_c[] = {
   { "CLC", NONE, 0x18 },
   { "CLD", NONE, 0xd8 },
   { "CLI", NONE, 0x58 },
   { "CLV", NONE, 0xb8 },
   { "CMP", IMM|ZP|ZP_X|ABS|ABS_X|ABS_Y|IND_X|IND_Y, 0xc1 },
   { "CPX", IMM2|ZP|ABS, 0xe0 },
   { "CPY", IMM2|ZP|ABS, 0xc0 },
   { "", 0, 0}
};

struct instruction inst_d[] = {
   { "DEC", ZP|ZP_X|ABS|ABS_X, 0xc2 },
   { "DEX", NONE, 0xca },
   { "DEY", NONE, 0x88 },
   { "", 0, 0}
};

struct instruction inst_e[] = {
   { "EOR", IMM|ZP|ZP_X|ABS|ABS_X|ABS_Y|IND_X|IND_Y, 0x41 },
   { "", 0, 0}
};

struct instruction inst_f[] = {
   { "", 0, 0}
};

struct instruction inst_g[] = {
   { "", 0, 0}
};

struct instruction inst_h[] = {
   { "", 0, 0}
};

struct instruction inst_i[] = {
   { "INC", ZP|ZP_X|ABS|ABS_X, 0xe2 },
   { "INX", NONE, 0xe8 },
   { "INY", NONE, 0xc8 },
   { "", 0, 0}
};

struct instruction inst_j[] = {
   { "JMP", ABS|ABS_IND, 0x40 },
   { "JSR", ABS, 0x14 },
   { "", 0, 0}
};

struct instruction inst_k[] = {
   { "", 0, 0}
};

struct instruction inst_l[] = {
   { "LDA", IMM|ZP|ZP_X|ABS|ABS_X|ABS_Y|IND_X|IND_Y, 0xa1 },
   { "LDX", IMM2|ZP|ZP_Y|ABS|ABS_Y2, 0xa2 },
   { "LDY", IMM2|ZP|ZP_X|ABS|ABS_X, 0xa0 },
   { "LSR", ACC|ZP|ZP_X|ABS|ABS_X, 0x42 },
   { "", 0, 0}
};

struct instruction inst_m[] = {
   { "", 0, 0}
};

struct instruction inst_n[] = {
   { "NOP", NONE, 0xea },
   { "", 0, 0}
};

struct instruction inst_o[] = {
   { "ORA", IMM|ZP|ZP_X|ABS|ABS_X|ABS_Y|IND_X|IND_Y, 0x01 },
   { "", 0, 0}
};

struct instruction inst_p[] = {
   { "PHA", NONE, 0x48 },
   { "PHP", NONE, 0x08 },
   { "PLA", NONE, 0x68 },
   { "PLP", NONE, 0x28 },
   { "", 0, 0}
};

struct instruction inst_q[] = {
   { "", 0, 0}
};

struct instruction inst_r[] = {
   { "ROL", ACC|ZP|ZP_X|ABS|ABS_X, 0x22 },
   { "ROR", ACC|ZP|ZP_X|ABS|ABS_X, 0x62 },
   { "RTI", NONE, 0x40 },
   { "RTS", NONE, 0x60 },
   { "", 0, 0}
};

struct instruction inst_s[] = {
   { "SBC", IMM|ZP|ZP_X|ABS|ABS_X|ABS_Y|IND_X|IND_Y, 0xe1 },
   { "SEC", NONE, 0x38 },
   { "SED", NONE, 0xf8 },
   { "SEI", NONE, 0x78 },
   { "STA", ZP|ZP_X|ABS|ABS_X|ABS_Y|IND_X|IND_Y, 0x81 },
   { "STX", ZP|ZP_Y|ABS, 0x82 },
   { "STY", ZP|ZP_X|ABS, 0x80 },
   { "", 0, 0}
};

struct instruction inst_t[] = {
   { "TAX", NONE, 0xaa },
   { "TAY", NONE, 0xa8 },
   { "TSX", NONE, 0xba },
   { "TXA", NONE, 0x8a },
   { "TXS", NONE, 0x9a },
   { "TYA", NONE, 0x98 },
   { "", 0, 0}
};

struct instruction *instructions[] = {
   inst_a,
   inst_b,
   inst_c,
   inst_d,
   inst_e,
   inst_f,
   inst_g,
   inst_h,
   inst_i,
   inst_j,
   inst_k,
   inst_l,
   inst_m,
   inst_n,
   inst_o,
   inst_p,
   inst_q,
   inst_r,
   inst_s,
   inst_t
};

static void error(const unsigned char);
static void fatal(const unsigned char);
void init(void); /* used by unit tests */
static void open_file(struct file *);
static void close_file(struct file *);
static void free_file_names(void);
static char *read_line(struct file *);
static int is_delimiter(const char);
static int is_comment(const char);
static void skip_delimiter(void);
#ifndef __CC65__
static char *strupper(char *);
#endif
struct instruction *get_command(void);
static int get_pseudo_func(void);
struct symbol *find_symbol(const char *); /* used by unit tests */
static struct symbol *find_local_symbol(const char *);
static void make_symbol(const char *, const unsigned int);
static void make_local_symbol(const char *, const unsigned int);
static void free_symbols(void);
static void print_symbols(void);
static struct symbol *get_symbol(const unsigned int);
static void swap_symbols(struct symbol *, struct symbol *);
static void sort_symbols(void);
static struct token *make_token(const unsigned int, const int, const char *, const struct instruction *);
static void free_tokens(void);
static char *get_string(void);
static unsigned int get_binary_number(void);
static unsigned int get_hex_number(void);
static unsigned int get_dec_number(void);
static char *get_identifier(void);
static char *get_local_identifier(void);
static struct token *get_token(void);
static int factor(void);
static int term(void);
static int expr(void);
static int eval1(const struct token *);
static int eval(void);
static void emit0(void);
static void emit1(const unsigned char);
static void emit2(const unsigned char, const unsigned char);
static void emit3(const unsigned char, const unsigned char, const unsigned char);
static char *parse_macro_param(void);
static void parse_macro_params(struct macro *);
static struct macro_param *get_macro_param(const struct macro *, const unsigned int);
static void expand_macro(const struct macro *);
static void free_macro_params(struct macro *);
static struct macro *find_macro(const char *);
static void make_macro(const char *);
static void print_macros(void);
static void free_macros(void);
void parse_line(void); /* used by unit tests */
static void handle_byte(void);
static void handle_else(void);
static void handle_endif(void);
static void handle_endmac(void);
static void handle_if(void);
static void handle_ifdef(void);
static void handle_ifndef(void);
static void handle_incbin(void);
static void handle_include(void);
static void handle_mac(void);
static void handle_org(void);
static void handle_word(void);
#ifndef __CC65__
static void usage(void);
static void parse_params(int argc, char *argv[]);
#endif

struct pseudo_func {
   char *name;
   void (*func)(void);
};

struct pseudo_func functions[] = {
   { ".BYTE", &handle_byte },
   { ".ELSE", &handle_else },
   { ".ENDIF", &handle_endif },
   { ".ENDMAC", &handle_endmac }, /* number 3 */
   { ".IF", &handle_if },
   { ".IFDEF", &handle_ifdef },
   { ".IFNDEF", &handle_ifndef },
   { ".INCBIN", &handle_incbin },
   { ".INCLUDE", &handle_include },
   { ".MAC", &handle_mac }, /* number 9 */
   { ".ORG", &handle_org },
   { ".WORD", &handle_word }
};

#define ENDMAC 3
#define MACRO 9

static char *read_mode = "r";
static char *write_mode = "w";

