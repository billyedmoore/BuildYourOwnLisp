#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>

#include "mpc.h"

#define LASSERT(args,cond,err)\
	if (!(cond)) {lval_del(args); return lval_err(err);}

typedef struct lval{
	int type;
	long num;
	char* err;// a string represeting the error
	char* sym;// the symbols
	int count;// the number of symbols
	struct lval** cell;// a pointer to pointers of children
} lval;

// DECLARATIONS
lval* lval_take(lval* v,int i );
lval* builtin_op(lval* a, char* op);
void lval_print(lval* v);
lval* lval_pop(lval* v,int i );
lval* lval_eval(lval* v);
lval* lval_eval_sexpr(lval* v);


enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR};

lval* lval_num(long x){
	// allocate memory the size of lval in the heap to store v
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

lval* lval_err(char* m){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(m)+1);
	strcpy(v->err, m); 
	return v;
}

lval* lval_sym(char* s){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s)+1);
	strcpy(v->sym,s);
	return v;
}

// create a lval for a sexpr and return a pointer to it
lval* lval_sexpr(void){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_qexpr(void){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}


void lval_del(lval* v){
	switch(v->type){
		case LVAL_NUM:
			break;
		case LVAL_ERR:
			free(v->err);
			break;
		case LVAL_SYM:
			free(v->sym);
			break;
		case LVAL_SEXPR:
		case LVAL_QEXPR:
			for(int i = 0; i < v->count; i++){
				lval_del(v->cell[i]);
			}
			free(v->cell);
			break;
	}
	free(v);
}

void lval_expr_print(lval* v, char open, char close){
	putchar(open);
	for (int i = 0; i < v->count; i++){
		lval_print(v->cell[i]);

		if (i!= (v->count-1)){
			putchar(' ');
		}
	}
	putchar(close);
}

void lval_print(lval* v){
	switch(v->type){
		case LVAL_NUM: printf("%li",v->num); break;
		case LVAL_ERR: printf("Error: %s", v->err); break;
		case LVAL_SYM: printf("%s", v->sym); break;
		case LVAL_SEXPR: lval_expr_print(v, '(',')'); break;
		case LVAL_QEXPR: lval_expr_print(v, '{','}');
	}
}

void lval_println(lval* v){
	lval_print(v);
	putchar('\n');
}

lval* lval_read_num(mpc_ast_t* t){
	errno = 0; 
	long x = strtol(t->contents,NULL,10);
	return errno != ERANGE?
		lval_num(x) : lval_err("invalid number");
}

lval* lval_add(lval* v, lval* x){
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	v->cell[v->count-1] = x;
	return v;
}

lval* lval_join(lval* x, lval* y){
	/**
	 * Move each child from y to x.
	 *
	 * lval* x: Lval to move from.
	 * lval* y: Lval to move to.
	 */

	while (y->count){
		lval_add(x,lval_pop(y,0));
	}

	lval_del(y);
	return x;
}

lval* builtin_join(lval* a){
	/**
	 * Take a q-expression and join each child into the first.
	 *
	 * lval* a: q-expression containing all of the chilren to be evaluated.
	 */
	for (int i=0; i<a->count ;i++){
		LASSERT(a,a->cell[i]->type == LVAL_QEXPR, 
				"Function 'join' passed Incorrect type");
	}

	lval* x = lval_pop(a,0);

	while( a->count){
		x = lval_join(x, lval_pop(a,0));
	}

	lval_del(a);
	return x;
}

lval* builtin_head(lval* a){
	/**
	 * Take a q-expression and return a q-expression with a sigular child 
	 * representing the first index.
	 *
	 * lval* a: q-expression to find the head of. 
	 */
	LASSERT(a,a->count == 1, "Incorrect number of arguments. (head)");
	LASSERT(a,a->cell[0]->type == LVAL_QEXPR , "Argument not of the correct type.");
	LASSERT(a,a->cell[0]->count != 0, "Passed {} to head");
	
	// take the first child from a and del a
	lval* v= lval_take(a,0);
	while(v->count > 1){
		lval_del(lval_pop(v,1));
	}
	return v;
}

lval* builtin_tail(lval* a){
	/**
	 * Take a q-expression and return a q-expression with the first index 
	 * removed
	 *
	 * lval* a: q-expression to find the head of. 
	 */
	LASSERT(a,a->count == 1 , "Incorrect number of arguments.(tail)");
	LASSERT(a,a->cell[0]->type == LVAL_QEXPR , "Argument not of the correct type.");
	LASSERT(a,a->cell[0]->count != 0, "Passed {} to tail");

	lval* v = lval_take(a, 0);
	lval_del(lval_pop(v,0));
	return v;
}

lval* builtin_list(lval* a){
	printf("list\n");
	a->type = LVAL_QEXPR;
	return a;
}

lval* builtin_eval(lval* a){
	LASSERT(a,a->count == 1 , "Incorrect number of arguments.(eval)");
	LASSERT(a,a->cell[0]->type == LVAL_QEXPR , "Argument not of the correct type.");
	
	// take the first child from a and delete a
	lval* v = lval_take(a, 0);
	// set the type of v to S-expression
	v->type = LVAL_SEXPR;
	// evaluate the S-expression
	return lval_eval(v);

}

lval* builtin_op(lval* a, char* op){
	for (int i = 0; i < a->count; i++){
		if (a->cell[i]->type != LVAL_NUM){
			lval_del(a);
			return lval_err("Cannot operate on non-number");
		}
	}

	lval* x = lval_pop(a,0);

	// if only one expression and - then negate
	if((strcmp(op,"-")==0) && a->count ==0){
		x->num = -x->num;
	}

	while (a->count > 0){
		lval* y = lval_pop(a,0);
		
		if (strcmp(op, "+") == 0){ x->num += y->num; }
		if (strcmp(op, "-") == 0){ x->num -= y->num; }
		if (strcmp(op, "*") == 0){ x->num *= y->num; }
		if (strcmp(op, "/") == 0){ 
			if (y->num == 0){
				lval_del(x);
				lval_del(y);
				x = lval_err("Division By Zero!");
				break;
			}
		x->num /= y->num;}
		lval_del(y);
	}
	lval_del(a);
	return x;
}

lval* builtin(lval* a, char* func){
	if( strcmp("list",func)==0){return builtin_list(a);}
	if( strcmp("eval",func)==0){return builtin_eval(a);}
	if( strcmp("head",func)==0){return builtin_head(a);}
	if( strcmp("tail",func)==0){return builtin_tail(a);}
	if( strcmp("join",func)==0){return builtin_join(a);}
	if( strstr("+-*/",func)){return builtin_op(a,func);}
	lval_del(a);
	return lval_err("Unkown function");
}

lval* lval_read(mpc_ast_t* t){

	if (strstr(t-> tag, "number")){
			return lval_read_num(t);}
	if (strstr(t-> tag, "symbol")){
			return lval_sym(t->contents);}

	lval* x = NULL;
	if (strcmp(t->tag,">") == 0) {x = lval_sexpr();}
	if (strstr(t->tag,"sexpr")) {x = lval_sexpr();}
	if (strstr(t->tag,"qexpr")) {x = lval_qexpr();}

	for (int i =0; i < t->children_num; i++){
		if (strcmp(t->children[i]->contents, "(") == 0){continue;}
		if (strcmp(t->children[i]->contents, ")") == 0){continue;}
		if (strcmp(t->children[i]->contents, "{") == 0){continue;}
		if (strcmp(t->children[i]->contents, "}") == 0){continue;}
		if (strcmp(t->children[i]->tag, "regex") == 0){continue;}
		x = lval_add(x,lval_read(t->children[i]));
	}
	return x;
}


lval* lval_pop(lval* v, int i){
	lval* x = v->cell[i];

	memmove(&v->cell[i], &v->cell[i+1],sizeof(lval*) * (v->count -1));
	v->count --;
	v-> cell = realloc(v->cell, sizeof(lval*) * v->count);
	return x;
}


lval* lval_take(lval* v,int i ){
	lval* x = lval_pop(v,i);
	lval_del(v);
	return x;
}


lval* lval_eval(lval* v){
	if (v->type == LVAL_SEXPR){
		return lval_eval_sexpr(v);
	}
	return v;}


lval* lval_eval_sexpr(lval* v){
	
	// eval all chilren
	for (int i = 0; i< v->count; i++){
		v->cell[i] = lval_eval(v->cell[i]);
	}
	
	// if a child throws error
	// take take that (del from v)
	// and return
	for (int i = 0; i< v->count; i++){
		if (v->cell[i]->type == LVAL_ERR){
			return lval_take(v,i);}
	}

	// if v has no chilren
	if(v->count == 0){return v;}
	
	// if v has one child take and return that
	if(v->count == 1){return lval_take(v,0);}
	
	// get first expre
	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_SYM){
		lval_del(f);
		lval_del(v);
		return lval_err("S-expression Doesn't start with symbol!");
	}
	
	// evaluate using builtin
	lval* result = builtin(v,f->sym); 
	lval_del(f);
	return result;
}






int main(int argc, char** argv){
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr = mpc_new("sexpr");
	mpc_parser_t* Qexpr = mpc_new("qexpr");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Lispy = mpc_new("lispy");
	
	// Language definition
	mpca_lang(MPCA_LANG_DEFAULT,
			"\
				number: /-?[0-9]+/;\
				symbol: \"list\" | \"head\" | \"tail\" |\
								\"join\" | \"eval\" |\
								'+' | '-' | '*' | '/';\
				sexpr: '(' <expr>* ')';\
				qexpr: '{' <expr>* '}';\
				expr: <number> | <symbol> | <sexpr> | <qexpr> ;\
				lispy: /^/ <expr>+ /$/;\
			",
			Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

	puts("Lispy Version 0.0.0.1");
	puts("Press Ctrl+C to Exit \n");
	
	while(1){
		char* input = readline("lispy >");
		add_history(input);

		mpc_result_t r;
		if (mpc_parse("<stdin>",input,Lispy,&r) ){
			lval* x = lval_eval(lval_read(r.output));
			lval_println(x);
			lval_del(x);
			mpc_ast_delete(r.output);
		} else{
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input);
	}
	mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
	return 0;
}


