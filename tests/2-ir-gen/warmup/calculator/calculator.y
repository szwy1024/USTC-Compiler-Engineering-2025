%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "syntax_tree.h"

// external functions from lex
extern int yylex();
extern int yyparse();
extern int yyrestart();
extern FILE * yyin;

// external variables from lexical_analyzer module
extern int lines;
extern char * yytext;
extern int pos_end;
extern int pos_start;

// Global syntax tree
syntax_tree *gt;

// Error reporting
void yyerror(const char *s);
syntax_tree_node *node(const char *node_name, int children_num, ...);
%}

%union {
     struct _syntax_tree_node * node;
     char * name;
}

%token <node> ADD
%token <node> SUB
%token <node> MUL
%token <node> DIV
%token <node> NUM
%token <node> LPARENTHESE
%token <node> RPARENTHESE
%type <node> input expression addop term mulop factor num

%start input

%%
// 如果不指定 <type>，Bison 使用 %union 中的第一个成员。
// %union 定义了 yylval 可用的语义类型，它是一个联合体
// 由%token <node>和%type <node>给出每个终结符和非终结符对应的语义值使用联合体中具体的哪个成员
// 如%token <node> NUM 表示使用 $1, $2, $3 等符号引用终结符的语义值时，转换为 node 类型（具体原理？）
// 如%type <node> input 表示使用 $$ 符号引用非终结符的语义值时，也转换为 node 类型，与node函数的返回值保持一致
// 使用$1,$2可以获取yylval.node中的值，yylval.node存放的是指向子结点的指针
// 在gt->root中存放指向input结点的指针，即指向语法树根结点的指针
input : expression {$$ = node( "input", 1, $1); gt->root = $$;}
    ;
expression : expression addop term  {$$ = node( "expression", 3, $1, $2, $3);}
    |   term {$$ = node( "expression", 1, $1);}
    ;

addop : ADD {$$ = node( "addop", 1, $1);}
    |  SUB {$$ = node( "addop", 1, $1);}
    ;

term : term mulop factor {$$ = node( "term", 3, $1, $2, $3);}
    |   factor {$$ = node( "term", 1, $1);}
    ;

mulop : MUL {$$ = node( "mulop", 1, $1);}
    |  DIV {$$ = node( "mulop", 1, $1);}
    ;

factor : LPARENTHESE expression RPARENTHESE {$$ = node( "factor", 3, $1, $2, $3);}
    |  num {$$ = node( "factor", 1, $1);}
    ;

num : NUM {$$ = node( "num", 1, $1);}
%%

void yyerror(const char * s) {
    fprintf(stderr, "error at line %d column %d: %s\n", lines, pos_start, s);
}

syntax_tree *parse(const char *input_path) {
    if (input_path != NULL) {
        if (!(yyin = fopen(input_path, "r"))) {
            fprintf(stderr, "[ERR] Open input file %s failed.\n", input_path);
            exit(1);
        }
    } else {
        yyin = stdin;
    }

    lines = pos_start = pos_end = 1;
    gt = new_syntax_tree();
    yyrestart(yyin);
    yyparse();
    return gt;
}

// 进行规约动作时，创建语法树中父结点的函数，返回指向父节点的指针
syntax_tree_node *node(const char *name, int children_num, ...) {
    syntax_tree_node *p = new_syntax_tree_node(name);
    syntax_tree_node *child;
    if (children_num == 0) {
        //如果没有子结点，子结点数字的第一个元素存入空串
        child = new_syntax_tree_node("epsilon");
        syntax_tree_add_child(p, child);
    } else {
        // 处理可变参数的宏，本质为一个指针
        va_list ap;
        va_start(ap, children_num);
        // 初始化ap，使其指向children_num下一个参数的位置
        for (int i = 0; i < children_num; ++i) {
            // 从当前指向的位置取出参数，转换其类型为syntax_tree_node *，然后指向下一个参数的位置
            child = va_arg(ap, syntax_tree_node *);
            // 将子结点添加到父结点的孩子结点数组中
            syntax_tree_add_child(p, child);
        }
        // 清理可变参数列表
        va_end(ap);
    }
    return p;
}
