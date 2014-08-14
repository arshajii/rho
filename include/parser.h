#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

Program *parse(Lexer *lex);

#endif /* PARSER_H */
