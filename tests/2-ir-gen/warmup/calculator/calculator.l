%option noyywrap
%{
/*****************声明和选项设置  begin*****************/
#include <stdio.h>
#include <stdlib.h>

#include "syntax_tree.h"
#include "calculator.h"

int lines;
int pos_start;
int pos_end;

// 创建一个叶子结点，yylval.node中保存指向这个结点的指针
void pass_node(char *text){
     yylval.node = new_syntax_tree_node(text);
}

/*****************声明和选项设置  end*****************/

// %x COMMENT定义了一个独占起始状态，但没有使用，使用方式如下
// "/*"    { BEGIN(COMMENT); }    匹配字符/*时，进入注释状态
// <COMMENT>"*/" { BEGIN(INITIAL); }   匹配字符*/时，退出注释状态
// <COMMENT>.    { /* 忽略注释内所有字符 */ }
// <COMMENT>\n   { /* 忽略注释内的换行 */ }
%}

%x COMMENT

%%

\+ 	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return ADD;}
\-	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return SUB;}
\*	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return MUL;}
\/	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return DIV;}
\(	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return LPARENTHESE;}
\)	{pos_start = pos_end; pos_end += 1; pass_node(yytext); return RPARENTHESE;}
[0-9]+ { pos_start = pos_end; pos_end += strlen(yytext); pass_node(yytext); return NUM;}

\n 	{lines++; pos_start = 1; pos_end = 1;}
[ \t] 	{pos_start = pos_end; pos_end += 1;}
%%
