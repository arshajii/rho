#ifndef RHO_LEXER_H
#define RHO_LEXER_H

#include <stdlib.h>
#include <stdbool.h>
#include "parser.h"

RhoToken *rho_parser_next_token(RhoParser *p);
RhoToken *rho_parser_next_token_direct(RhoParser *p);
RhoToken *rho_parser_peek_token(RhoParser *p);
RhoToken *rho_parser_peek_token_direct(RhoParser *p);
bool rho_parser_has_next_token(RhoParser *p);
void rho_parser_tokenize(RhoParser *p);
const char *rho_type_to_str(RhoTokType type);

#endif /* RHO_LEXER_H */
