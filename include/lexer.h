#ifndef LEXER_H
#define LEXER_H

#include <stdlib.h>
#include <stdbool.h>
#include "parser.h"

Token *parser_next_token(Parser *p);
Token *parser_next_token_direct(Parser *p);
Token *parser_peek_token(Parser *p);
Token *parser_peek_token_direct(Parser *p);
bool parser_has_next_token(Parser *p);
void parser_tokenize(Parser *p);
const char *type_to_str(TokType type);

#endif /* LEXER_H */
