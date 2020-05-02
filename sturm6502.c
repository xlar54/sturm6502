/*
 *
 *    Sturm6502
 *    Minimal macro assembler for 6502
 *
 *    Coded by Juha Ollila
 *
 */

#include "stdio.h"
#include "stdlib.h"
#include "ctype.h"
#include "string.h"

#include "sturm6502.h"


int eval();
struct macro *find_macro(char *);
void parse_line(void);

struct file *file_cur; /* current file */
struct file file_asm;
struct file file_out;
struct file file_lst;
struct file file_bin; /* incbin file */

char line[MAX_LINE_LEN];
char cline[MAX_LINE_LEN]; /* line with preversed case */
unsigned char column, pass;
unsigned char opt_debug, opt_list, opt_symbol;
unsigned char if_supress, if_seen;
unsigned int PC;
struct symbol *symbols, *last_symbol, *last_label;
struct token *tokens, *last_token, *tok_global;
struct macro *macros, *last_macro;

/* for unit tests */
char unit_obj[3];
unsigned char unit_asm_fails;

void error(unsigned char err) {
   #ifdef UNIT_TEST
   unit_asm_fails += 1;
   printf("%s", error_msg[err]);
   #else
   printf("%s:%d %s\n", file_cur->name, file_cur->line_number, error_msg[err]);
   exit(1);
   #endif
}

void init(void) {
   unit_asm_fails = 0;
   column = 0;
   PC = 0;
   pass = 0;
   symbols = NULL;
   last_symbol = NULL;
   tokens = NULL;
   last_token = NULL;
   macros = NULL;
   last_macro = NULL;
   last_label = NULL;
   opt_debug = 0;
   opt_list = 0;
   opt_symbol = 0;
   if_supress = 0;
   if_seen = 0;
   file_asm.mode = strdup("r");
   file_lst.mode = strdup("w");
   file_lst.name = strdup("a.lst");
   file_bin.mode = strdup("r");
   file_out.mode = strdup("w");
   file_out.name = strdup("a.out");
}

void open_file(register struct file *file) {
   file->line_number = 0;
   if ((file->file = fopen(file->name, file->mode)) == NULL) {
      // error(OPEN_ERROR);
      printf("Cannot open file: %s\n", file->name);
      exit(1);
   }
}

void close_file(register struct file *file) {
   fclose(file->file);
}

char *read_line(register struct file *file) {
   char *input;
   column = 0;
   file->line_number++;
   input = fgets(line, MAX_LINE_LEN, file->file);
   if (input == NULL && ferror(file->file))
      error(READ_ERROR);
   return input;
}

int is_delimiter(char c) {
   unsigned char i;
   for (i = 0; i < (unsigned char) strlen(delimiter_chars); i++)
      if (c == delimiter_chars[i]) return 1;
   return 0;
}

int is_comment(char c) {
   unsigned char i;
   for (i = 0; i < (unsigned char) strlen(comment_chars); i++)
      if (c == comment_chars[i]) return 1;
   return 0;
}

void skip_delimiter(void) {
   while (is_delimiter(line[column]))
      column++;
}

#ifndef __CC65__
char *strupper(char *str) {
   char *result;
   result = str;
   while (*str != 0) {
      *str = toupper(*str);
      str++;
   }
   return result;
}
#endif

int get_command(void) {
   unsigned char start = column;
   unsigned char i;
   while (isalpha(line[column]))
      column++;
   for (i = 0; i < (sizeof(instructions) / sizeof(struct instruction)); i++) {
      if (strncmp(&line[start], instructions[i].mnemonic, 3) == 0 && !isalnum(line[start+3])) {
         return i;
      }
   }
   column = start;
   return NOT_FOUND;
}

int get_pseudo_func(void) {
   unsigned char start = column;
   unsigned char i;
   column++;
   while (isalpha(line[column]))
      column++;
   for (i = 0; i < (sizeof(functions) / sizeof(struct pseudo_func)); i++) {
      if (strncmp(&line[start], functions[i].name, column-start) == 0) {
         return i;
      }
   }
   column = start;
   return NOT_FOUND;
}

struct symbol *find_symbol(char *name) {
   struct symbol *next_symbol = symbols;
   while (next_symbol) {
      if (strcmp(name, next_symbol->name) == 0)
         return next_symbol;
      next_symbol = next_symbol->global;
   }
   return NULL;
}

struct symbol *find_local_symbol(char *name) {
   struct symbol *next_symbol;
   if (last_label)
     next_symbol = last_label->local;
   else
      return NULL;
   while (next_symbol) {
      if (strcmp(name, next_symbol->name) == 0)
         return next_symbol;
      next_symbol = next_symbol->local;
   }
   return NULL;
}

void make_symbol(char *name, unsigned int value) {
   struct symbol *old_symbol, *new_symbol;
   char *symbol_name;
   /* if symbol already exists then update it */
   if((old_symbol=find_symbol(name))!=0) {
      old_symbol->value = value;
      return;
   }
   /* new symbol */
   new_symbol = malloc(sizeof(struct symbol));
   symbol_name = strdup(name);
   if (new_symbol && symbol_name) {
      if (last_symbol) last_symbol->global = new_symbol; else symbols = new_symbol;
      new_symbol->global = NULL;
      new_symbol->local = NULL;
      new_symbol->name = symbol_name;
      new_symbol->value = value;
      last_symbol = new_symbol;
   }
}

void make_local_symbol(char *name, unsigned int value) {
   struct symbol *old_symbol, *new_symbol, *last_local;
   char *symbol_name;
   /* if symbol already exists then update it */
   if((old_symbol=find_local_symbol(name))!=0) {
      old_symbol->value = value;
      return;
   }
   /* new symbol */
   new_symbol = malloc(sizeof(struct symbol));
   symbol_name = strdup(name);
   if (last_label)
     last_local = last_label;
   else
      error(SYNTAX_ERROR);
   if (new_symbol && symbol_name) {
      while(last_local->local)
         last_local = last_local->local;
      last_local->local = new_symbol;
      new_symbol->global = NULL;
      new_symbol->local = NULL;
      new_symbol->name = symbol_name;
      new_symbol->value = value;
   }
}

void free_symbols(void) {
   struct symbol *next_symbol = symbols;
   struct symbol *current_symbol, *local_symbol;
   while (next_symbol) {
      /* free local symbols */
      local_symbol = next_symbol->local;
      while(local_symbol) {
         if (local_symbol->name) free(local_symbol->name);
         current_symbol = local_symbol;
         local_symbol = local_symbol->local;
         free(current_symbol);
      }
      /* free global symbol */
      current_symbol = next_symbol;
      next_symbol = current_symbol->global;
      if (current_symbol->name) free(current_symbol->name);
      free(current_symbol);
   }
}

void print_symbols(void) {
   struct symbol *symbol1, *symbol2;
   symbol1 = symbols;
   printf("*** Symbols ***\n");
   while (symbol1) {
      symbol2 = symbol1->global;
      if (symbol1 && symbol2) {
         printf("%20s %04X %20s %04X\n", symbol1->name, symbol1->value, symbol2->name, symbol2->value);
         symbol1 = symbol2->global;
      } else if (symbol1) {
         printf("%15s %04X\n", symbol1->name, symbol1->value);
         symbol1 = NULL;
      }
   }
}

struct symbol *get_symbol(unsigned int j) {
   struct symbol *sym = symbols;
   unsigned int i = 0;
   while(i < j && sym) {
      i++;
      sym = sym->global;
   }
   return sym;
}

void swap_symbols(struct symbol *sym1, struct symbol *sym2) {
   struct symbol tmp;
   tmp.value = sym1->value;
   tmp.name = sym1->name;
   tmp.local = sym1->local;
   sym1->value = sym2->value;
   sym1->name = sym2->name;
   sym1->local = sym2->local;
   sym2->value = tmp.value;
   sym2->name = tmp.name;
   sym2->local = tmp.local;
};

/* simple (slow) insertion sort */
void sort_symbols(void) {
   unsigned int i = 1;
   unsigned int j;
   unsigned int len = 0;
   struct symbol *sym = symbols;
   /* calculate len */
   while (sym) {
      len++;
      sym = sym->global;
   }
   /* actual insertion sort */
   while (i < len) {
      j = i;
      while (j > 0 && (get_symbol(j-1)->value) > (get_symbol(j)->value)) {
         swap_symbols(get_symbol(j), get_symbol(j-1));
         j--;
      }
      i++;
   }
}


struct token *make_token(unsigned int id, int value, char *label) {
   struct token *new_tok = malloc(sizeof(struct token));
   if (new_tok) {
      if (last_token) last_token->next = new_tok; else tokens = new_tok;
      new_tok->id = id;
      new_tok->value = value;
      new_tok->label = label;
      new_tok->next = NULL;
   }
   return new_tok;
}

void free_tokens(void) {
   struct token *next_token = tokens;
   struct token *tok;
   while (next_token) {
      tok = next_token;
      next_token = tok->next;
      if (tok->label) free(tok->label);
      free(tok);
   }
   tokens = NULL;
   last_token = NULL;
}

char *get_string() {
   char *str;
   unsigned char len = 0;
   unsigned char start;
   column++;
   start=column;
   while ((line[column] != '"') && (line[column] != 0)) {
      len++;
      column++;
   }
   if (line[column] == '"')
      column++;
   str = malloc(len+1);
   str[len] = 0;
   if (str)
      strncpy(str, &line[start], len);
   return str;
}

unsigned int get_binary_number() {
   unsigned int value = 0;
   column++;
   while (line[column] == '1' || line[column] == '0') {
      value *= 2;
      value += (line[column]-'0');
      column++;
   }
   return value;
}

unsigned int get_hex_number() {
   unsigned int value = 0;
   column++;
   while (isxdigit(line[column])) {
      value *= 16;
      if (isdigit(line[column])) {
         value += (line[column]-'0');
      } else {
         value += (line[column]-'A'+10);
      }
      column++;
   }
   return value;
}

unsigned int get_dec_number() {
   unsigned int value = 0;
   while (isdigit(line[column])) {
      value *= 10;
      value += (line[column]-'0');
      column++;
   }
   return value;
}

char *get_identifier() {
   char *name;
   unsigned char len = 0;
   unsigned char start = column;
   while (isalnum(line[column])) {
      len++;
      column++;
   }
   // skip trailing column
   if (line[column]==':')
      column++;
   name = malloc(len+1);
   if (name) {
      name[len] = 0;
      strncpy(name, &line[start], len);
   }
   return name;
}

char *get_local_identifier() {
   char *name;
   unsigned char len = 0;
   unsigned char start = column;
   while (isalnum(line[column]) || line[column] == '@') {
      len++;
      column++;
   }
   // skip trailing column
   if (line[column]==':')
      column++;
   name = malloc(len+1);
   if (name) {
      name[len] = 0;
      strncpy(name, &line[start], len);
   }
   return name;
}

struct token *get_token() {
   int value, cmd;
   char *str, *label;
   struct symbol *sym;
   struct macro *mac;
   /* comment */
   if (column == 0) {
      if(is_comment(line[column]))
         return make_token(TOKEN_COMMENT, 0, 0);
   }
   skip_delimiter();
   /* string */
   if (line[column] == '"') {
      str = get_string();
      return make_token(TOKEN_STRING, 0, str);
   }
   /* number */
   if (line[column] == '%') {
      value = get_binary_number();
      return make_token(TOKEN_NUMBER, value, 0);
   }
   if (line[column] == '$') {
      value = get_hex_number();
      return make_token(TOKEN_NUMBER, value, 0);
   }
   if (isdigit(line[column])) {
      value = get_dec_number();
      return make_token(TOKEN_NUMBER, value, 0);
   }
   /* command */
   if ((cmd=get_command()) != NOT_FOUND) {
      return make_token(TOKEN_CMD, cmd, strdup(instructions[cmd].mnemonic)); 
   }
   /* a bunch of other tokens */
   switch(line[column]) {
      case 'A':
         if(!isalnum(line[column+1])) {
            column++;
            return make_token(TOKEN_A, 0, 0);
         }
         break;
      case 'X':
         if(!isalnum(line[column+1])) {
            column++;
            return make_token(TOKEN_X, 0, 0);
         }
         break;
      case 'Y':
         if(!isalnum(line[column+1])) {
            column++;
            return make_token(TOKEN_Y, 0, 0);
         }
         break;
      case '\'': 
         if(isprint(line[column+1])) {
            value = line[column+1];
            if (line[column+2] == '\'') column += 3; else column += 2;
            return make_token(TOKEN_CHAR, value, 0);
         }
         break;
      case 0:
         return make_token(TOKEN_EOL, 0, 0);
      case '\n':
         column++;
         return make_token(TOKEN_EOL, 0, 0);
      case '\r':
         column++;
         return make_token(TOKEN_EOL, 0, 0);
      case '#':
         column++;
         return make_token(TOKEN_HASH, 0, 0);
      case ',':
         column++;
         return make_token(TOKEN_COMMA, 0, 0);
      case '(':
         column++;
         return make_token(TOKEN_LPAREN, 0, 0);
      case ')':
         column++;
         return make_token(TOKEN_RPAREN, 0, 0);
      case '*':
         column++;
         return make_token(TOKEN_MUL, 0, 0);
      case '<':
         column++;
         return make_token(TOKEN_LOW, 0, 0);
      case '>':
         column++;
         return make_token(TOKEN_HIGH, 0, 0);
      case '+':
         column++;
         return make_token(TOKEN_PLUS, 0, 0);
      case '-':
         column++;
         return make_token(TOKEN_MINUS, 0, 0);
      case '/':
         column++;
         return make_token(TOKEN_DIV, 0, 0);
      case '=':
         column++;
         return make_token(TOKEN_EQU, 0, 0);
      case ';':
         column++;
         return make_token(TOKEN_COMMENT, 0, 0);
      case '.':
         if ((cmd = get_pseudo_func()) != NOT_FOUND) {
            return make_token(TOKEN_PSEUDO, cmd, strdup(functions[cmd].name)); 
         } else error(INVALID_CMD);
   }
   /* identifier */
   if (isalpha(line[column])) {
      label = get_identifier();
      mac = find_macro(label);
      if (mac)
         return make_token(TOKEN_IDENT, 0, label);
      sym = find_symbol(label);
      if (sym)
         return make_token(TOKEN_IDENT, sym->value, label);
      /* 32767 is dummy value for the forward reference */
      if (pass == 1)
         return make_token(TOKEN_IDENT, 32767, label);
   }
   /* local identifier */
   if (line[column] == '@') {
      label = get_local_identifier();
      sym = find_local_symbol(label);
      if (sym)
         return make_token(TOKEN_LOCAL, sym->value, label);
      /* forward reference */
      if (pass == 1)
         return make_token(TOKEN_LOCAL, 32767, label);
   }
   return make_token(TOKEN_UNKNOWN, 0, 0);
}

void expect_token(unsigned int id) {
   struct token *tok = get_token();
   if (tok->id != id)
      error(SYNTAX_ERROR);
}

int factor() {
   int l_value = 0;
   if(opt_debug >= 3)
      printf("factor() tok_global->id %d\n", tok_global->id);
   if (tok_global->id == TOKEN_IDENT) {
      l_value = tok_global->value;
      tok_global = get_token();
   } else if (tok_global->id == TOKEN_LOCAL) {
      l_value = tok_global->value;
      tok_global = get_token();
   } else if (tok_global->id == TOKEN_NUMBER) {
      l_value = tok_global->value;
      tok_global = get_token();
   } else if (tok_global->id == TOKEN_CHAR) {
      l_value = tok_global->value;
      tok_global = get_token();
   } else if (tok_global->id == TOKEN_STRING) {
      l_value = tok_global->label[0];
      tok_global = get_token();
   } else if (tok_global->id == TOKEN_LPAREN) {
      l_value = eval();
      expect_token(TOKEN_RPAREN);
   } else {
      error(INVALID_EXPR);
   }
   if(opt_debug >= 3)
      printf("factor() l_value %d\n", l_value);
   return l_value;
}

int term() {
   int l_value;
   if(opt_debug >= 3)
      printf("term() #1\n");
   l_value = factor();
   while (tok_global->id == TOKEN_MUL || tok_global->id == TOKEN_DIV) {
      if(opt_debug >= 3)
         printf("term() #2\n");
      if (tok_global->id == TOKEN_MUL) {
         tok_global = get_token();
         l_value *= factor();
      } else {
         tok_global = get_token();
         l_value = (int) (l_value / factor());
      }
   }
   return l_value;
}

int expr() {
   int l_value;
   if(opt_debug >= 3)
      printf("expr() #1\n");
   if (tok_global->id == TOKEN_MINUS) {
      tok_global = get_token();
      l_value = -term();
   } else {
      l_value = term();
   }
   while (tok_global->id == TOKEN_MINUS || tok_global->id == TOKEN_PLUS) {
      if(opt_debug >= 3)
         printf("expr() #2\n");
      if (tok_global->id == TOKEN_MINUS) {
         tok_global = get_token();
         l_value -= term();
      } else {
         tok_global = get_token();
         l_value += term();
      }
   }
   return l_value;
}

int eval1(struct token *tok) {
   int l_value;
   tok_global = tok;
   if (tok_global->id == TOKEN_HIGH) {
      tok_global = get_token();
      l_value = (int) (expr() / 256);
   } else if (tok_global->id == TOKEN_LOW) {
      tok_global = get_token();
      l_value = expr() % 256;
   } else {
      l_value = expr();
   }
   return l_value;
}

int eval() {
   return eval1(get_token());
}

void emit0(void) {
   #ifndef UNIT_TEST
   if (opt_debug >= 2)
      printf("%05d %04X          %s", file_cur->line_number, PC, &cline[0]);
   if (pass == 2 & opt_list == 1)
      if (fprintf(file_lst.file, "%05d %04X          %s", file_cur->line_number, PC, &cline[0]) < 0)
         error(WRITE_ERROR);
   #endif
}

void emit1(unsigned char b1) {
   #ifdef UNIT_TEST
   unit_obj[0]=b1;
   unit_obj[1]=0xf2;
   unit_obj[2]=0xf2;
   #else
   if (opt_debug >= 2)
      printf("%05d %04X %02X       %s", file_cur->line_number, PC, b1, &cline[0]);
   if (pass == 2 & opt_list == 1)
      if (fprintf(file_lst.file, "%05d %04X %02X       %s", file_cur->line_number, PC, b1, &cline[0]) < 0)
         error(WRITE_ERROR);
   if (pass == 2)
      if (fputc(b1, file_out.file) == EOF) error(WRITE_ERROR);
   #endif
   PC = PC + 1;
}

void emit2(unsigned char b1, unsigned char b2) {
   #ifdef UNIT_TEST
   unit_obj[0]=b1;
   unit_obj[1]=b2;
   unit_obj[2]=0xf2;
   #else
   if (opt_debug >= 2)
      printf("%05d %04X %02X %02X    %s", file_cur->line_number, PC, b1, b2, &cline[0]);
   if (pass == 2 & opt_list == 1)
      if (fprintf(file_lst.file, "%05d %04X %02X %02X    %s", file_cur->line_number, PC, b1, b2, &cline[0]) < 0)
         error(WRITE_ERROR);
   if (pass == 2) {
      if (fputc(b1, file_out.file) == EOF) error(WRITE_ERROR);
      if (fputc(b2, file_out.file) == EOF) error(WRITE_ERROR);
   }
   #endif
   PC = PC + 2;
}

void emit3(unsigned char b1, unsigned char b2, unsigned char b3) {
   #ifdef UNIT_TEST
   unit_obj[0]=b1;
   unit_obj[1]=b2;
   unit_obj[2]=b3;
   #else
   if (opt_debug >= 2)
      printf("%05d %04X %02X %02X %02X %s", file_cur->line_number, PC, b1, b2, b3, &cline[0]);
   if (pass == 2 & opt_list == 1)
      if (fprintf(file_lst.file, "%05d %04X %02X %02X %02X %s", file_cur->line_number, PC, b1, b2, b3, &cline[0]) < 0)
         error(WRITE_ERROR);
   if (pass == 2) {
      if (fputc(b1, file_out.file) == EOF) error(WRITE_ERROR);
      if (fputc(b2, file_out.file) == EOF) error(WRITE_ERROR);
      if (fputc(b3, file_out.file) == EOF) error(WRITE_ERROR);
   }
   #endif
   PC = PC + 3;
}

char *parse_macro_param() {
   char *param;
   unsigned char len = 0;
   unsigned char start = column;
   while (cline[column] && cline[column]!=',' && cline[column]!='\n' && cline[column]!='\r' ) {
      len++;
      column++;
   }
   // skip trailing comma
   if (cline[column]==',')
      column++;
   if (len == 0)
      return 0;
   param = malloc(len+1);
   param[len] = 0;
   if (param)
      strncpy(param, &cline[start], len);
   return param;
}

void parse_macro_params(struct macro *mac) {
   char *str;
   struct macro_param *new_param, *last_param;
   while((str = parse_macro_param()) != 0) {
      new_param = malloc(sizeof(struct macro_param));
      if(new_param) {
         new_param->str = str;
         new_param->next = NULL;
         if (mac->param == NULL) mac->param = new_param; else last_param->next = new_param;
         last_param = new_param;
      } else error(OUT_OF_MEMORY);
   }
}

struct macro_param *get_macro_param(struct macro *mac, unsigned int j) {
   struct macro_param *param;
   unsigned int i = 0;
   if (j > 9)
      return NULL;
   param = mac->param;
   while(i < j && param) {
      i++;
      param = param->next;
   }
   return param;
}

void expand_macro(struct macro *mac) {
   unsigned char c, num, src_len, i, j, k;
   struct macro_param *param;
   struct macro_line *mac_line;
   mac_line = mac->line;
   while (mac_line) {
      src_len = strlen(mac_line->str);
      i = 0;
      j = 0;
      while(i < src_len) {
         c = mac_line->str[i++];
         if(c == '\\') {
            num = mac_line->str[i++] - '1';
            param = get_macro_param(mac, num);
            k = 0;
            while(param && k < strlen(param->str))
               line[j++] = param->str[k++];
        } else {
           line[j++] = c;
        }
     }
     line[j] = 0;
     strcpy(&cline[0], &line[0]);
     strupper(&line[0]);
     column = 0;
     if (opt_debug >= 2)
        printf("%s", &line[0]);
     parse_line();
     free_tokens();
     mac_line = mac_line->next;
  }
}

void free_macro_params(struct macro *mac) {
   struct macro_param *next_param, *current_param;
   next_param = mac->param;
   while(next_param) {
      free(next_param->str);
      current_param = next_param;
      next_param = next_param->next;
      free(current_param);
   }
   mac->param = NULL;
}

struct macro *find_macro(char *name) {
   struct macro *next_macro = macros;
   while (next_macro) {
      if (strcmp(name, next_macro->name) == 0)
         return next_macro;
      next_macro = next_macro->next;
   }
   return NULL;
}

void make_macro(char *name) {
   struct macro *new_macro;
   char *macro_name;
   char *text;
   struct macro_line *last_line;
   struct macro_line *new_line;
   struct token *tok;
   emit0();
   /* if pass 2 then skip macro definition */
   while(pass == 2 && read_line(file_cur) != 0) {
      /* check of end macro definition */
      strcpy(&cline[0], &line[0]);
      emit0();
      column = 0;
      strupper(&line[0]);
      tok = get_token();
      if(opt_debug >= 2)
         printf("skipping macro\n");
      if (tok->id == TOKEN_PSEUDO && tok->value == ENDMAC)
         return;
   }
   /* check if macro already exist */
   if((new_macro=find_macro(name))!=0)
      error(INVALID_MACRO);
   /* new macro */
   new_macro = malloc(sizeof(struct macro));
   macro_name = strdup(name);
   if (new_macro && macro_name) {
      if (last_macro) last_macro->next = new_macro; else macros = new_macro;
      new_macro->name = macro_name;
      new_macro->param = NULL;
      new_macro->line = NULL;
      new_macro->local = NULL;
      new_macro->next = NULL;
      last_macro = new_macro;
      /* process each line */
      last_line = new_macro->line;
      while(read_line(file_cur) != 0) {
         /* check of end macro definition */
         column = 0;
         strcpy(&cline[0], &line[0]);
         strupper(&line[0]);
         tok = get_token();
         if (tok->id == TOKEN_PSEUDO && tok->value == ENDMAC)
            return;
         /* copy line */
         text = strdup(&cline[0]);
         new_line = malloc(sizeof(struct macro_line));
         if (text && new_line) {
            if (last_line) last_line->next = new_line; else new_macro->line = new_line;
            new_line->str = text;
            new_line->next = NULL;
            last_line = new_line;
         } else error(OUT_OF_MEMORY);
      }
   } else error(OUT_OF_MEMORY);
}

void print_macros(void) {
   struct macro *next_macro;
   struct macro_line *line;
   next_macro = macros;
   printf("*** Macros  ***\n");
   while (next_macro) {
      printf("Macro %s:\n", next_macro->name);
      line = next_macro->line;
      while(line) {
         printf("%s", line->str);
         line = line->next;
      }
      next_macro = next_macro->next;
   }
}

void free_macros(void) {
   struct macro *next_macro = macros;
   struct macro *current_macro;
   struct macro_line *next_line, *current_line;
   while (next_macro) {
      next_line = next_macro->line;
      while(next_line) {
         free(next_line->str);
         current_line = next_line;
         next_line = next_line->next;
         free(current_line);
      }
      current_macro = next_macro;
      next_macro = next_macro->next;
      free(current_macro);
   }
}

void parse_line(void) {
   struct token *tok_prev, *tok;
   int value;
   struct instruction inst;
   unsigned char code;
   struct macro *mac;
   struct macro_param *next_param;
   tok = get_token();
   /* empty line or comment line */
   if (tok->id == TOKEN_EOL || tok->id == TOKEN_COMMENT) {
      emit0();
      return;
   }
   /* identifier i.e. equ, macro or label */
   if (tok->id == TOKEN_IDENT && if_supress == 0) {
      tok_prev = tok;
      mac = find_macro(tok_prev->label);
      if (mac) {
         if (opt_debug >= 2)
            printf("Expand macro: %d\n", file_cur->line_number);
         parse_macro_params(mac);
         expand_macro(mac);
         free_macro_params(mac);
         return;
      }
      tok = get_token();
      if (tok->id == TOKEN_EQU) {
         value = eval();
         make_symbol(tok_prev->label, value);
         tok = tok_global;
      } else {
         make_symbol(tok_prev->label, PC);
         last_label = find_symbol(tok_prev->label);
      }
   /* local label */
   } else if (tok->id == TOKEN_LOCAL && if_supress == 0) {
      make_local_symbol(tok->label, PC);
      tok = get_token();
   }
   /* check eol and comment */
   if (tok->id == TOKEN_EOL || tok->id == TOKEN_COMMENT) {
      emit0();
      return;
   }
   /* pseudo function */
   if (tok->id == TOKEN_PSEUDO) {
      if (tok->value == MACRO) {
         tok = get_token();
         if (tok->id == TOKEN_IDENT) {
            make_macro(tok->label);
         } else error(SYNTAX_ERROR);
      } else {
         functions[tok->value].func();
      }
      return;
   }
   /* check .if supressing is ongoing */
   if (if_supress == 1)
      return;
   /* mnemonic */
   if (tok->id == TOKEN_CMD) {
      inst = instructions[tok->value];
      tok = get_token();
      if (tok->id == TOKEN_EOL ||
          tok->id == TOKEN_COMMENT ||
          tok->id == TOKEN_A)
      {
         /* implied mode */
         if (inst.addr_modes & NONE) {
            code = inst.op_code;
            if (opt_debug >= 3) printf("implied mode: %x\n", code);
            emit1(code);
         /* accumulator mode */
         } else if (inst.addr_modes & ACC) {
            code = inst.op_code + 8;
            if (opt_debug >= 3) printf("accumulator mode: %x\n", code);
            emit1(code);
         } else {
            error(INVALID_MODE);
         }
         return; // new
      /* immediate mode */
      } else if (tok->id == TOKEN_HASH) {
         value = eval();
         if (inst.addr_modes & IMM) {
            code = inst.op_code + 8;
            if (opt_debug >= 3) printf("immediate mode1: %x %x\n", code, value);
            emit2(code, value%256);
         } else if (inst.addr_modes & IMM2) {
            code = inst.op_code + 0;
            if (opt_debug >= 3) printf("immediate mode2: %x %x\n", code, value);
            emit2(code, value%256);
         } else {
            error(INVALID_MODE);
         }
         tok = tok_global; // new
      /* relative mode */
      } else if (inst.addr_modes & REL) {
         code = inst.op_code + 0;
         value = eval1(tok);
         value = value - PC - 2;
         if (pass == 2 && (value < -128 || value > 127))
            error(INVALID_BRANCH);
         if ( value < 0)
            value = 256 + value;
         if (opt_debug >= 3) printf("relative mode: %x %x\n", code, value);
         emit2(code, value%256);
         tok = tok_global; // new
      } else if (tok->id == TOKEN_LPAREN) {
         value = eval();
         tok = tok_global;
         /* (indirect,x) */
         if (tok->id == TOKEN_COMMA &&
             get_token()->id == TOKEN_X &&
             get_token()->id == TOKEN_RPAREN &&
             (inst.addr_modes & IND_X))
         {
            code = inst.op_code + 0;
            if (opt_debug >= 3) printf("(indirect,x) mode: %x %x\n", code, value);
            emit2(code, value%256);
         /* (absolute indirect */
         } else if (tok->id == TOKEN_RPAREN &&
                    (inst.addr_modes & ABS_IND))
         {
            code = inst.op_code + 0x2c;
            if (opt_debug >= 3) printf("(absolute indirect) mode: %x %x %x\n", code, value%256, value/256);
            emit3(code, value%256, value/256);
         /* (indirect),y */
         } else if (tok->id == TOKEN_RPAREN &&
                    get_token()->id == TOKEN_COMMA &&
                    get_token()->id == TOKEN_Y &&
                    (inst.addr_modes & IND_Y))
         {
            code = inst.op_code + 0x10;
            if (opt_debug >= 3) printf("(indirect),y mode: %x %x\n", code, value);
            emit2(code, value%256);
         } else {
            error(INVALID_MODE);
         }
         tok = get_token(); // new
      /* addressing mode should be zp, zp_x, zp_y, abs, abs_x, abs_y, or abs_y2 */
      } else {
         value = eval1(tok);
         tok = tok_global;
         /* addressing mode should be zp_x, zp_y, abs_x, abs_y, or abs_y2 */
         if (tok->id == TOKEN_COMMA) {
            tok = get_token();
            /* addressing mode should be zp_x or abs_x */
            if (tok->id == TOKEN_X) {
               if (value < 256 && (inst.addr_modes & ZP_X)) {
                  code = inst.op_code + 0x14;
                  if (opt_debug >= 3) printf("zp_x mode: %x %x\n", code, value);
                  emit2(code, value);
               } else if (inst.addr_modes & ABS_X) {
                  code = inst.op_code + 0x1c;
                  if (opt_debug >= 3) printf("abs_x mode: %x %x %x\n", code, value%256, value/256);
                  emit3(code, value%256, value/256);
               } else {
                  error(INVALID_MODE);
               }
            /* addressing mode should be zp_y, abs_y, or abs_y2 */
            } else if (tok->id == TOKEN_Y) {
               if (value < 256 && (inst.addr_modes & ZP_Y)) {
                  code = inst.op_code + 0x14;
                  if (opt_debug >= 3) printf("zp_y mode: %x %x\n", code, value);
                  emit2(code, value);
               } else if (inst.addr_modes & ABS_Y) {
                  code = inst.op_code + 0x18;
                  if (opt_debug >= 3) printf("abs_y mode: %x %x %x\n", code, value%256, value/256);
                  emit3(code, value%256, value/256);
               } else if (inst.addr_modes & ABS_Y2) {
                  code = inst.op_code + 0x1c;
                  if (opt_debug >= 3) printf("abs_y2 mode: %x %x %x\n", code, value%256, value/256);
                  emit3(code, value%256, value/256);
               } else {
                  error(INVALID_MODE);
               }
            } else {
               error(INVALID_MODE);
            }
            tok = get_token(); // new
         /* addressing mode should be zp or abs */
         } else {
            /* zp addressing mode */
            if (value < 256 && (inst.addr_modes & ZP)) {
               code = inst.op_code + 0x4;
               if (opt_debug >= 3) printf("zp mode: %x %x\n", code, value);
               emit2(code, value);
            /* absolute addressing mode */
            } else if (inst.addr_modes & ABS ) {
               code = inst.op_code + 0xc;
               if (opt_debug >= 3) printf("absolute mode: %x, %x, %x\n", code, value%256, value/256);
               emit3(code, value%256, value/256);
            } else {
               error(INVALID_MODE);
            }
         }
      }
   } else {
      error(INVALID_CMD);
   }
   /* check eol and comment */
   if (tok->id != TOKEN_EOL && tok->id != TOKEN_COMMENT)
      error(SYNTAX_ERROR);
}

void handle_byte(void) {
   struct token *tok;
   unsigned char i;
   int value;
   do {
      tok = get_token();
      if (tok->id == TOKEN_STRING) {
         for (i = 0; i < strlen(tok->label); i++) {
            emit1(tok->label[i]);
         }
         tok = get_token();
      } else {
         value = eval1(tok);
         tok = tok_global;
         emit1(value%256);
      }
   } while (tok->id == TOKEN_COMMA);
}

void handle_else(void) {
   emit0();
   if (if_seen == 1) {
      if (if_supress == 0)
         if_supress = 1;
      else if_supress = 0;
   } else error(SYNTAX_ERROR);
}

void handle_endif(void) {
   emit0();
   if (if_seen == 1) {
      if_supress = 0;
      if_seen = 0;
   } else error(SYNTAX_ERROR);
}

void handle_endmac(void) {
   /* This call should not happen */
   /* make_macro handles .mac */
   error(INTERNAL_ERROR);
}

void handle_if(void) {
   int value;
   emit0();
   value = eval();
   if_seen = 1;
   if (value == 0)
      if_supress = 1;
   else
      if_supress = 0;
}

void handle_ifdef(void) {
   char *label;
   struct symbol *sym = symbols;
   emit0();
   skip_delimiter();
   label = get_identifier();
   if_seen = 1;
   while (sym) {
      if (strcmp(sym->name, label)==0) {
         if_supress = 0;
         return;
      }
      sym = sym->global;
   }
   if_supress = 1;
}

void handle_ifndef(void) {
   char *label;
   struct symbol *sym = symbols;
   emit0();
   skip_delimiter();
   label = get_identifier();
   if_seen = 1;
   while (sym) {
      if (strcmp(sym->name, label)==0) {
         if_supress = 1;
         return;
      }
      sym = sym->global;
   }
   if_supress = 0;
}

void handle_incbin(void) {
   struct token *tok;
   int input;
   if (if_supress == 1)
      return;
   emit0();
   tok = get_token();
   if (tok->id == TOKEN_STRING) {
      file_bin.name = tok->label;
      open_file(&file_bin);
      while((input = fgetc(file_bin.file)) != EOF)
         emit1(input);
      close_file(&file_bin);
      if (ferror(file_bin.file))
         error(READ_ERROR);
   } else error(SYNTAX_ERROR);
}

void handle_include(void) {
   struct token *tok;
   struct file file_inc;
   struct file *file_parent;

   if (if_supress == 1)
      return;
   emit0();
   tok = get_token();
   if (tok->id == TOKEN_STRING) {
      file_parent = file_cur;
      file_cur = &file_inc;
      file_cur->name = tok->label;
      file_cur->mode = "r";
      open_file(file_cur);
      while(read_line(file_cur) != 0) {
         strcpy(&cline[0], &line[0]);
         strupper(&line[0]);
         column = 0;
         parse_line();
         free_tokens();
      }
      close_file(file_cur);
      file_cur = file_parent;
   } else error(SYNTAX_ERROR);
}

void handle_mac(void) {
   /* This call should not happen */
   /* make_macro handles .mac */
   error(INTERNAL_ERROR);
}

void handle_org(void) {
   if (if_supress == 1)
      return;

   PC = eval();
   emit0();
}

void handle_word(void) {
   int value;
   if (if_supress == 1)
      return;

   do {
      value = eval();
      emit2(value%256, value/256); 
   } while (tok_global->id == TOKEN_COMMA);
}

void not_imp(void) {
   printf("Not implemented yet.\n");
}

void usage(void) {
   printf("Sturm6502 macro assembler\n\n");
   printf("Usage:          sturm6502 [options] sourcefile\n\n");
   printf("-d #            debug level\n");
   printf("-D label=value  define a numeric constant\n");
   printf("-l name         generate list file\n");
   printf("-o name         output file name\n");
   printf("-s              print symbols\n\n");
}

void parse_params(int argc, char *argv[]) {
   unsigned char i = 1;
   if (argc < 2) {
      usage();
      exit(1);
   }
   file_asm.name = argv[argc-1]; // 2
   while (i < argc - 1) { // i = 1
      if (strncmp("-d", argv[i], 2) == 0 && (i < argc - 2)) {
         opt_debug = argv[i+1][0] - '0';
         i += 2;
      } else if (strncmp("-D", argv[i], 2) == 0 && (i < argc - 2)) {
         strcpy(&line[0], argv[i+1]);
         parse_line();
         i += 2;
      } else if (strncmp("-l", argv[i], 2) == 0 && i < (argc -2 )) {
         opt_list = 1;
         file_lst.name = argv[i+1];
         i += 2;
      } else if (strncmp("-o", argv[i], 2) == 0 && i < (argc -2 )) {
         file_out.name = argv[i+1];
         i += 2;
      } else if (strncmp("-s", argv[i], 2) == 0) {
      printf("s");
         opt_symbol = 1;
         i += 1;
      } else {
         usage();
         exit(1);
      }
   }
   if (opt_debug == 1) {
      printf("*** Options ***\n");
      printf("Debug option:  %d\n", opt_debug);
      printf("List option:   %d\n", opt_list);
      printf("Symbol option: %d\n", opt_symbol);
      printf("List file:     %s\n", file_lst.name);
      printf("Output file:   %s\n", file_out.name);
      printf("Source file:   %s\n", file_asm.name);
      print_symbols();
   }
}

#ifndef UNIT_TEST
int main(int argc, char *argv[]) {

   char *str;
   init();
   parse_params(argc, argv);
   file_cur = &file_asm;
   open_file(file_cur);
   open_file(&file_out);
   pass = 1;
   printf("*** Pass #1 ***\n");
   while((str = read_line(file_cur)) != 0) {
      strcpy(&cline[0], &line[0]);
      strupper(&line[0]);
      column = 0;
      parse_line();
      free_tokens();
   }
   PC = 0;
   pass = 2;
   last_label = NULL;
   printf("*** Pass #2 ***\n");
   close_file(file_cur);
   open_file(file_cur);
   if (pass == 2 && opt_debug >= 2) {
      printf("*** Listing ***\n");
      printf("Line# Loc  Code     Line\n");
   }
   if (pass == 2 && opt_list == 1) {
      open_file(&file_lst);
      if (fprintf(file_lst.file, "*** Listing ***\n") < 0) error(WRITE_ERROR);
      if (fprintf(file_lst.file, "Line# Loc  Code     Line\n") < 0) error(WRITE_ERROR);
   }
   while((str = read_line(file_cur)) != 0) {
      strcpy(&cline[0], &line[0]);
      strupper(&line[0]);
      column = 0;
      parse_line();
      free_tokens();
   }
   close_file(file_cur);
   close_file(&file_out);
   if (pass == 2 && opt_list == 1)
      close_file(&file_lst);
   if (opt_symbol) {
      sort_symbols();
      print_symbols();
   }
   if(opt_debug >= 1)
      print_macros();
   free_macros();
   free_symbols();
   return 0;
}
#endif
