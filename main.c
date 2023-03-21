#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>

#include "mpc.h"

#define LASSERT(args,cond,err,...)\
	if (!(cond)) {\
		lval* error = lval_err(err,##__VA_ARGS__);\
		lval_del(args); \
		return error; } 

#define LASSERT_TYPE(func_name,a,index,intended_type){\
		LASSERT(a,a->cell[index]->type == intended_type, \
				"The function %s expected %s but got %s",\
				func_name, ltype_name(intended_type), \
				ltype_name(a->cell[index]->type)); \
	}

#define LASSERT_NUM(func_name,a,intended_num){ \
		LASSERT(a,a->count == intended_num, \
				"The function %s got the incorrect number of args." \
				"Got %i instead of %i.", \
				func_name,\
				a->count,\
				intended_num);\
}

#define LASSERT_NOT_EMPTY(func_name,a){\
	LASSERT(a,a->count != 0,\
			"The function %s was passed an empty expr.", \
			func_name)\
}
// Forward definition of lval and lenv
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/**
 * Define a function pointer to a function that takes an lenv* and an lval* 
 * and returns an lval*.
 */
typedef lval*(*lbuiltin)(lenv*, lval*);

enum {LVAL_NUM, LVAL_STR, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR};


char* ltype_name(int t){
	switch(t){
		case LVAL_NUM: return "Number";
		case LVAL_STR: return "String";
		case LVAL_ERR: return "Error";
		case LVAL_SYM: return "Symbol";
		case LVAL_FUN: return "Function";
		case LVAL_SEXPR: return "S-Expression";
		case LVAL_QEXPR: return "Q-Expression";
		default: return "Unknown";
		
	}
}
struct lval{
	int type;  // The type, one of the values from the above enum. 
	long num; // Used by LVAL_NUM.
	char* err;// Used by LVAL_ERR.
	char* sym;// Used by LVAL_SYM.
	char* str;// Used by LVAL_STR.

	// Function
	lbuiltin builtin; // Used by LVAL_FUN. Used by LVAL_SEXPR AND LVAL_QEXPR. 
	lenv* env;
	lval* formals;
	lval* body;

	int count;// The number of children.
	lval** cell;// The chilren, each child is an lval*.
};

struct lenv{
	lenv* parent;
	int count;
	char** syms;
	lval** vals;
};

// DECLARATIONS
lval* lval_err(char* m,...);
lval* lval_fun(lbuiltin func);
lval* lval_sym(char* name);
lval* lval_copy(lval* v);
lval* lval_add(lval* v, lval* x);
lval* lval_take(lval* v,int i );
lval* builtin_op(lenv* env, lval* a, char* op);
void lval_print(lval* v);
lval* lval_pop(lval* v,int i );
lval* lval_eval(lenv* env,lval* v);
void lval_del(lval* v);
lval* lval_eval_sexpr(lenv* env,lval* v);

lval* builtin_lambda(lenv* e, lval* v);
lval* builtin_join(lenv* env,lval* a);
lval* builtin_head(lenv* env,lval* a);
lval* builtin_tail(lenv* env,lval* a);
lval* builtin_eval(lenv* env,lval* a);
lval* builtin_list(lenv* env,lval* a);
lval* builtin_neq(lenv* env, lval* a);
lval* builtin_eq(lenv* env,lval* a);
lval* builtin_if(lenv* env, lval* a);

lval* builtin_var(lenv* env, lval* a,char* func);
lval* builtin_def(lenv* env, lval* a);
lval* builtin_put(lenv* env, lval* a);

lval* builtin_mul(lenv* env, lval* a);
lval* builtin_sub(lenv* env, lval* a);
lval* builtin_add(lenv* env, lval* a);
lval* builtin_div(lenv* env, lval* a);

lval* builtin_ge(lenv* env,lval* a);
lval* builtin_gt(lenv* env,lval* a);
lval* builtin_le(lenv* env,lval* a);
lval* builtin_lt(lenv* env,lval* a);

lenv* lenv_new(void){
	/**
	 * Create a new enviroment.
	 *
	 * returns:
	 * lenv* The new enviroment.
	 */
	lenv* enviroment = malloc(sizeof(lenv));
	enviroment->parent = NULL;
	enviroment->count = 0;
	enviroment->syms = NULL;
	enviroment->vals = NULL;
	return enviroment;
};

void lenv_del(lenv* e){
	/**
	 * Deletes an lenv.
	 *
	 * lenv* e: the lenv to delete.
	 */
	for(int i = 0; i< e->count; i++){
		free(e->syms[i]);
		lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

lenv* lenv_copy(lenv* env){
	/**
	 * Make a copy of an enviroment.
	 *
	 * lenv* env: The enviroment to copy.
	 * Returns:
	 * lenv*: a pointer to the copy.
	 */
	lenv* new = lenv_new();
	new->parent = env->parent;
	new->count = env->count;
	new->syms = malloc(sizeof(char*) * new->count);
	new->vals = malloc(sizeof(lval*) * new->count);
	for (int i = 0; i< new->count; i++){
		new->syms[i] = malloc(strlen(env->syms[i])+1);
		strcpy(new->syms[i],env->syms[i]);
		new->vals[i] = lval_copy(env->vals[i]);
	}
	return new;
}

lval* lenv_get(lenv* env, lval* key){
	/**
	 * Get the value bound to a key in an given enviroment.
	 *
	 * lenv* env: The enviroment where to look for the value.
	 * lval* key: An lval* of type LVAL_SYM.
	 */
	for(int i = 0; i < env->count; i++){
		if(strcmp(env->syms[i],key->sym) == 0){
			return lval_copy(env->vals[i]);
		}
	}
	if (env->parent){
		return lenv_get(env->parent,key);
	}
	return lval_err("Unbound symbol! '%s'",key->sym);
}

void lenv_put(lenv* env, lval* key, lval* value){
	/**
	 * Bound a new local variable. 
	 * Copies the content so orignal memory can be freed.
	 * 
	 * lenv* env: The enviroment to where variables are being stored.
	 * lval* key: The symbol, should be of type LVAL_SYM.
	 * lval* value: The value. Can be of any type?
	 */
	for(int i = 0; i < env->count; i++){
		// If key already in the enviroment.
		if(strcmp(env->syms[i],key->sym) == 0){
			// Delete the value.
			lval_del(env->vals[i]);
			// Replace the value with a copy of the new value.
			env->vals[i] = lval_copy(value);
			return;
		}
	}
	
	// Allocate space for new entry.
	env->count++;
	env->vals = realloc(env->vals,sizeof(lval*) * env->count);
	env->syms = realloc(env->syms,sizeof(char*) * env->count);
	
	// Copy the contents into the new memory locations.
	env->vals[env->count - 1] = lval_copy(value);
	env->syms[env->count - 1] = malloc(strlen(key->sym)+1);
	strcpy(env->syms[env->count-1],key->sym);
}

void lenv_def(lenv* env, lval* key, lval* value){
	/**
	 * Bound a new global variable. 
	 * Copies the content so orignal memory can be freed.
	 * 
	 * lenv* env: The enviroment to where variables are being stored.
	 * lval* key: The symbol, should be of type LVAL_SYM.
	 * lval* value: The value. Can be of any type?
	 */
	//travese to the root of the tree of enviroments
	while(env->parent){env = env->parent;}
	// define the varible in that eviroment
	lenv_put(env,key,value);
}

void lenv_add_builtin(lenv* env, char* name, lbuiltin func){
	lval* key = lval_sym(name);
	lval* value = lval_fun(func);

	lenv_put(env,key,value);
	lval_del(key);
	lval_del(value);
}


void lenv_add_builtins(lenv* env){
	lenv_add_builtin(env,"tail",builtin_tail);	
	lenv_add_builtin(env,"head",builtin_head);	
	lenv_add_builtin(env,"eval",builtin_eval);	
	lenv_add_builtin(env,"join",builtin_join);	
	lenv_add_builtin(env,"list",builtin_list);	
	lenv_add_builtin(env,"def",builtin_def);	
	lenv_add_builtin(env,"=",builtin_put);	

	lenv_add_builtin(env,"plus",builtin_add);	
	lenv_add_builtin(env,"sub",builtin_sub);	
	lenv_add_builtin(env,"times",builtin_mul);	
	lenv_add_builtin(env,"div",builtin_div);	
	
	lenv_add_builtin(env,"+",builtin_add);	
	lenv_add_builtin(env,"-",builtin_sub);	
	lenv_add_builtin(env,"*",builtin_mul);	
	lenv_add_builtin(env,"/",builtin_div);	

	lenv_add_builtin(env,"==", builtin_eq);
	lenv_add_builtin(env,"!=", builtin_neq);

	lenv_add_builtin(env,">", builtin_gt);
	lenv_add_builtin(env,">=", builtin_ge);
	lenv_add_builtin(env,"<", builtin_lt);
	lenv_add_builtin(env,"<=", builtin_le);
	
	
	lenv_add_builtin(env, "if", builtin_if);
	lenv_add_builtin(env, "\\", builtin_lambda);
}

// LVAL CONSTRUCTORS

lval* lval_lambda(lval* formals,lval* body){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	
	// Set builtin to NULL as not builtin.
	v->builtin = NULL;
	
	// New enviroment.
	v->env = lenv_new();

	v->formals = formals;
	v->body = body;
	return v;
}

lval* lval_num(long x){
	/**
	 * Returns a pointer to a new lval of type LVAL_NUM. 
	 *
	 * long x: The number to be represented.
	 */
	// allocate memory the size of lval in the heap to store v
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

lval* lval_str(char* str){
	/**
	 * Returns a pointer to a new lval of type LVAL_STR.
	 *
	 * char* str: The str to be represented.
	 */
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_STR;
	v->str = malloc(strlen(str)+1);
	strcpy(v->str,str);
	return v;
}

lval* lval_err(char* format,...){
	/**
	 * Returns a pointer to a new lval of type LVAL_ERR
	 *
	 * char* format: The error message as a printf style format.
	 * ...: the varibles to be pushed into the string.
	 */
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;

	va_list va;
	va_start(va, format);

	v->err = malloc(512);

	vsnprintf(v->err, 511, format, va);
	v->err = realloc(v->err, strlen(v->err)+1);
	va_end(va);

	return v;

}

lval* lval_sym(char* s){
	/**
	 * Returns a pointer to a new lval of type LVAL_SYM
	 *
	 * char* s: The body of the symbol.
	 */
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s)+1);
	strcpy(v->sym,s);
	return v;
}

// create a lval for a sexpr and return a pointer to it
lval* lval_sexpr(void){
	/**
	 * Returns a pointer to a new lval of type LVAL_SEXPR
	 */
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_qexpr(void){
	/**
	 * Returns a pointer to a new lval of type LVAL_SEXPR
	 */
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_fun(lbuiltin func){
	/**
	 * Returns a pointer to a new lval of type LVAL_FUN
	 */
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->builtin = func;
	return v; 
}

// LVAL UTIL FUNCTIONS/METHODS

void lval_del(lval* v){
	/**
	 * Deletes an lval* object. Works deeply i.e. also deletes all chilren.
	 *
	 * lval* v: The lval* to be deleted.
	 */
	switch(v->type){
		case LVAL_NUM:
			break;
		case LVAL_STR:
			free(v->str);
			break;
		case LVAL_ERR:
			free(v->err);
			break;
		case LVAL_SYM:
			free(v->sym);
			break;
		case LVAL_FUN:
			if (!v->builtin){
				lenv_del(v->env);
				lval_del(v->formals);
				lval_del(v->body);
			}
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
void lval_print_str(lval* v){
	/**
	 * Escape a string and then print it.
	 *
	 * lval* v: Of type of LVAL_STR.
	 */
	// Make a copy of the string.
	char* escaped = malloc(strlen(v->str)+1);
	strcpy(escaped,v->str);
	// Escape using an mpc function.
	escaped = mpcf_escape(escaped);
	// Print.
	printf("\"%s\"",escaped);
	// Delete the copy.
	free(escaped);
}


void lval_print(lval* v){
	switch(v->type){
		case LVAL_NUM: printf("%li",v->num); break;
		case LVAL_ERR: printf("Error: %s", v->err); break;
		case LVAL_SYM: printf("%s", v->sym); break;
		case LVAL_FUN: 
			if (v->builtin){
				printf("<builtin>");
			}
			else{
				printf("(\\");
				lval_print(v->formals);
				putchar(' ');
				lval_print(v->body);
				putchar(')');
			}
			break;
		case LVAL_STR: lval_print_str(v); break;
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

lval* lval_copy(lval* v){
	/**
	 * Return a deepcopy or clone of a lval.
	 s
	 * lval* v: The lval* to copy.
	 *
	 * Returns: The copy.
	 */

	lval* x = malloc(sizeof(lval));
	x->type = v->type;

	switch (x->type){
		case LVAL_FUN: 
			if (v->builtin){
				x->builtin = v->builtin;
			}
			else{
				x->builtin = NULL;
				x->env = lenv_copy(v->env); // to be defined
				x->formals = lval_copy(v->formals);
				x->body = lval_copy(v->body);
			}
			break;
		case LVAL_NUM:
			x->num = v->num;
			break;
		case LVAL_STR:
			x->str = malloc(strlen(v->str)+1);
			strcpy(x->str,v->str); 
		case LVAL_SYM:
			x->sym = malloc(strlen(v->sym)+1);
			strcpy(x->sym,v->sym);
			break;
		case LVAL_ERR:
			x->err = malloc(strlen(v->err)+1);
			strcpy(x->err,v->err);
			break;
		case LVAL_QEXPR:
		case LVAL_SEXPR:
			x->count = v->count;
			x->cell = malloc(sizeof(lval*) * x->count);
			for (int i = 0; i < x->count; i++){
					x->cell[i] = lval_copy(v->cell[i]);
			}
			break;
	}
	return x;
}
lval* lval_call(lenv* env, lval* function, lval* args){
	/**
	 * Call an the function represented by an lval* object of type
	 * LVAL_FUN. Returns a partially initialised fuction if (number of 
	 * arguments) < (expected number of arguments).
	 *
	 * lenv* env: The global enviroment.
	 * lval* function: The function lval* of type LVAL_FUN.
	 * lval* args: The Q-expression representing the arguments.
	 *
	 * Returns:
	 * lval*: the result of the fuction call. Or the partially initialised 
	 * 	     function
	 */
	if (function->builtin){
		return function->builtin(env,args);
	}
	
	int given = args->count;
	int total = function->formals->count;
	
	while (args->count){

		// If no more arguments to bind.
		if (function->formals->count == 0){
			lval_del(args);
			return lval_err("Function passed too many arguments. "
					"Got %i expected %i. "
					"Evaluated %i.",given,total,given-(args->count));
		}

		// Pop the next "formal".
		lval* symbol = lval_pop(function->formals,0);
	
		// & means the function accepts a varible number of args.
		if (strcmp(symbol->sym,"&")){
	
			if (function->formals->count != 0){
				lval_del(args);
				return lval_err("The '&' symbol not followed by exactly one symbol.");
			}
			
			lval* next_symbol = lval_pop(function->formals, 0);
			// The remaining args will still be in args;
			lenv_put(function->env,next_symbol,builtin_list(env,args));
			lval_del(args);
			lval_del(next_symbol);
		}
	
		// Pop the next argument.
		lval* arg = lval_pop(args,0);

		lenv_put(function->env,symbol,arg);
			
		// Delete the symbols.
		lval_del(symbol);
		lval_del(arg);
	}
	
	//Delete the args lval* as all args have been evaluated.
	lval_del(args);
	
	// If there is no arguments left and there is '&' followed by a symbol.
	if(function->formals->count > 0 && 
		strcmp(function->formals->cell[0]->sym,"&") == 0){
		
		// If the & symbol is not followed by one symbol.
		if (function->formals->count != 2){
				lval_del(args);
				return lval_err("The '&' symbol not followed by exactly one symbol.");
			}
		
		// Delete the lval represeting '&'.
		lval_del(lval_pop(function->formals,0));
		
		lval* sym = lval_pop(function->formals,0);
		lval* value = lval_qexpr();
		
		lenv_put(function->env,sym,value);
		lval_del(sym);
		lval_del(sym);
	}

	if(function->formals->count == 0){
		function->env->parent = env;
		return builtin_eval(function->env,
				lval_add(lval_sexpr(),lval_copy(function->body)));
	}
	else{
		return lval_copy(function);
	}
	
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

int lval_eq(lval* x, lval* y){
	/**
	 * Compares two lval objects, returns 1 if they are equal
	 * and 0 if not.
	 *
	 * lval* x Lval to compare.
	 * lval* y Lval to compare.
	 * Returns:
	 * int Whether the lvals are equal.
	 */

	if(x->type != y->type){
		return 0;
	}

	switch(x->type){
	case LVAL_NUM:
		return x->num == y->num;
	case LVAL_STR:
		return (strcmp("x->str","y->str") == 0);
	case LVAL_SYM:
		return (strcmp("x->sym","y->sym") == 0);
	case LVAL_ERR:
		return (strcmp("x->err","y->err") == 0);
	case LVAL_SEXPR:
	case LVAL_QEXPR:
		if (x->count != y->count){
			return 0;
		}
		for (int i=0; i < x->count; i++){
			// If any pair of elements are not the same return false.
			if (!lval_eq(x->cell[i],y->cell[i])){
				return 0;
			}
		}
		return 1;
	case LVAL_FUN:
		if (x->builtin || y->builtin){
			return (x->builtin == y->builtin);}
		else {
			return (lval_eq(x->formals,y->formals) && 
							lval_eq(x->body,y->body));
		}
		break;
	}
	return 0;
}

lval* builtin_if(lenv* env, lval* a){
	/**
	 * A bulitin if function. Works like the terniary operator in c.
	 *
	 * Example usage: 
	 *  if (a == b) {def {a} 1} {def {a} 2}
	 *
	 * lenv* env: The enviroment where to 
	 * lval* a: The arguments. Should be made up of LVAL_NUM, 
	 * 					LVAL_QEXPR, LVAL_QEXPR. Where the LVAL_NUM 
	 * 					represents a boolean value.
	 */
	
	// Check for correct number of args.
	LASSERT_NUM("if",a,3);
	// Check for correct types
	LASSERT_TYPE("if",a,0,LVAL_NUM);
	LASSERT_TYPE("if",a,1,LVAL_QEXPR);
	LASSERT_TYPE("if",a,2,LVAL_QEXPR);
	
	lval* x;
	// Change the qexpr to sexpr so they can be executed.
	a->cell[1]->type = LVAL_SEXPR;
	a->cell[2]->type = LVAL_SEXPR;
	
	// execute cell[1]
	if (a->cell[0]->num){
		x = lval_eval(env,lval_pop(a,1));
	}
	// execute cell[2]
	else{
		x =lval_eval(env, lval_pop(a,2));
	}

	lval_del(a);
	return x; 
}

lval* builtin_ord(lenv* env, lval* a, char* op){
	LASSERT_NUM(op,a,2);
	LASSERT_TYPE(op,a,0,LVAL_NUM);
	LASSERT_TYPE(op,a,1,LVAL_NUM);
	int r;

	if (strcmp(op,">=") == 0){
		r =  (a->cell[0]->num >= a->cell[1]->num);
	}
	if (strcmp(op,">") == 0){
		r =  (a->cell[0]->num > a->cell[1]->num);
	}
	if (strcmp(op,"<=") == 0){
		r = (a->cell[0]->num <= a->cell[1]->num);
	}
	if (strcmp(op,"<") == 0){
		r = (a->cell[0]->num < a->cell[1]->num);
	}
	
	lval_del(a);
	return lval_num(r);
}

lval* builtin_ge(lenv* env,lval* a){
	return builtin_ord(env,a,">=");
}

lval* builtin_gt(lenv* env,lval* a){
	return builtin_ord(env,a,">");
}

lval* builtin_le(lenv* env,lval* a){
	return builtin_ord(env,a,"<=");
}

lval* builtin_lt(lenv* env,lval* a){
	return builtin_ord(env,a,"<");
}

lval* builtin_cmp(lenv* env, lval* a, char* op){
	/**
	 * Function to compare two arguments, used for 
	 * builtin_eq and builtin_neq.
	 *
	 * lenv* env: The enviroment where to exectute.
	 * lval* a: The arguments passed.
	 * char* op: One of '>', '>=', '<' and '<='.
	 */
	LASSERT_NUM(op,a,2);
	int r;
	lval* x= lval_pop(a,0);
	lval* y= lval_pop(a,0);

	if (strcmp(op,"==") == 0){
		r = lval_eq(x,y);
	}
	if (strcmp(op,"!=") == 0){
		r = !lval_eq(x,y);
	}

	lval_del(x);
	lval_del(y);
	lval_del(a);
	
	return lval_num(r);
}

lval* builtin_eq(lenv* env, lval* a){
	return builtin_cmp(env,a,"==");
}

lval* builtin_neq(lenv* env, lval* a){
	return builtin_cmp(env,a,"!=");
}

lval* builtin_def(lenv* env,lval* a){
	/**
	 */

	return builtin_var(env,a,"def");
}

lval* builtin_put(lenv* env, lval* a){
	/**
	 */
	return builtin_var(env,a,"=");

}

lval* builtin_lambda(lenv* e, lval* v){
	/**
	 * Takes an lval* representing a lambda and returns a new 
	 * lval of type LVAL_FUN.
	 * 
	 * lenv* e: The enviroment.
	 * lval* v: The lval of type LVAL_SEXPR.
	 *
	 * Returns:
	 * lval*: An lval* of type LVAL_FUN.
	 */

	// Check that the lval has 2 children both of which are Q-Expressions
	LASSERT_NUM("//", v, 2);
	LASSERT_TYPE("//", v, 0, LVAL_QEXPR);
	LASSERT_TYPE("//", v, 1, LVAL_QEXPR);
	
	// Check each of the formals is a Symbol
	for (int i =0; i < v->cell[0]->count; i++){
		LASSERT(v,(v->cell[0]->cell[i]->type) == LVAL_SYM,
				"Cannot define non-symbol. Got %s expected %s.",
				ltype_name(v->cell[0]->cell[i]->type),
				ltype_name(LVAL_SYM));
	}

	lval* formals = lval_pop(v,0);
	lval* body = lval_pop(v,0);
	lval_del(v);

	return lval_lambda(formals,body);
}

lval* builtin_var(lenv* env, lval* a,char* func){
	/**
	 * Bind a list (q-expression) of symbols to the corresponding
	 * arguments.
	 *
	 * lenv* env: The enviroment where to execute.
	 * lval a: The arguemnts.
	 * char* func: Either "=" or "def" determines which scope to define vars in 
	 * 						 local or global respectively.
	 */ 
	// checks the first value is a q-expression
	LASSERT_TYPE("def", a, 0, LVAL_QEXPR);
	
	lval_print(a);
	// list of symbols
	lval* syms = a->cell[0]; 
	
	// Check all the elements are symbols.
	for(int i=0; i < syms->count; i++){
			LASSERT_TYPE("def", syms, i , LVAL_SYM);
	}

	LASSERT(a, syms->count == a->count-1, 
			"There are a different number of values to symbols."
			"Got %i and expected %i.",syms->count,a->count-1);

	for(int i = 0; i< syms->count; i++){
		if (strcmp(func,"def") == 0){
			lenv_def(env,syms->cell[i],a->cell[i+1]);
		}
		else if (strcmp(func,"=") == 0){
			lenv_put(env,syms->cell[i],a->cell[i+1]);
		}
	}

	lval_del(a);
	return lval_sexpr();
}

lval* lval_read_str(mpc_ast_t* t){
	/**
	 * Read a string into an lval_str. Unescapes the string.
	 *
	 * mpc_ast_t* t: The string node from the mpc library.
	 */
	t->contents[strlen(t->contents) -1] = '\0';
	char* unescaped = malloc(strlen(t->contents+1)+1);
	strcpy(unescaped,t->contents+1);
	unescaped = mpcf_unescape(unescaped);
	lval* str = lval_str(unescaped);

	free(unescaped);
	return str;
}

lval* builtin_join(lenv* env,lval* a){
	/**
	 * Take a q-expression and join each child into the first.
	 *
	 * lenv* env: The enviroment.
	 * lval* a: q-expression containing all of the chilren to be evaluated.
	 */
	for (int i=0; i<a->count ;i++){
		LASSERT_TYPE("join", a, i, LVAL_QEXPR);
	}

	lval* x = lval_pop(a,0);

	while( a->count){
		x = lval_join(x, lval_pop(a,0));
	}

	lval_del(a);
	return x;
}

lval* builtin_head(lenv* e, lval* a){
	/**
	 * Take a q-expression and return a q-expression with a sigular child 
	 * representing the first index.
	 *
	 * lenv* env: The enviroment.
	 * lval* a: q-expression to find the head of. 
	 */
	LASSERT_NUM("head", a, 1);
	LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
	LASSERT(a,a->cell[0]->count != 0, "Passed {} to head");
	LASSERT_NOT_EMPTY("head",a );
	
	// take the first child from a and del a
	lval* v= lval_take(a,0);
	while(v->count > 1){
		lval_del(lval_pop(v,1));
	}
	return v;
}

lval* builtin_tail(lenv* env, lval* a){
	/**
	 * Take a q-expression and return a q-expression with the first index 
	 * removed
	 *
	 * lenv* env: The enviroment.
	 * lval* a: q-expression to find the head of. 
	 */
	LASSERT_NUM("tail", a, 1);
	LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
	LASSERT_NOT_EMPTY("head",a );

	lval* v = lval_take(a, 0);
	lval_del(lval_pop(v,0));
	return v;
}

lval* builtin_list(lenv* env,lval* a){
	printf("list\n");
	a->type = LVAL_QEXPR;
	return a;
}

lval* builtin_eval(lenv* env,lval* a){
	LASSERT_NUM("eval", a, 1);
	LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);
	
	// take the first child from a and delete a
	lval* v = lval_take(a, 0);
	// set the type of v to S-expression
	v->type = LVAL_SEXPR;
	// evaluate the S-expression
	return lval_eval(env,v);

}

lval* builtin_add(lenv* env, lval* a){ 
	return builtin_op(env,a,"+");
}

lval* builtin_sub(lenv* env, lval* a){ 
	return builtin_op(env,a,"-");
}

lval* builtin_div(lenv* env, lval* a){ 
	return builtin_op(env,a,"/");
}

lval* builtin_mul(lenv* env, lval* a){ 
	return builtin_op(env,a,"*");
}

lval* builtin_op(lenv* env, lval* a, char* op){
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

lval* builtin(lenv* env,lval* a, char* func){
	if( strcmp("list",func)==0){return builtin_list(env,a);}
	if( strcmp("eval",func)==0){return builtin_eval(env,a);}
	if( strcmp("head",func)==0){return builtin_head(env,a);}
	if( strcmp("tail",func)==0){return builtin_tail(env,a);}
	if( strcmp("join",func)==0){return builtin_join(env,a);}
	if( strstr("+-*/",func)){return builtin_op(env,a,func);}
	lval_del(a);
	return lval_err("Unkown function");
}

lval* lval_read(mpc_ast_t* t){

	if (strstr(t-> tag, "number")){
			return lval_read_num(t);}
	if (strstr(t-> tag, "symbol")){
			return lval_sym(t->contents);}
	if (strstr(t->tag ,"string")){
			return lval_read_str(t);
	}

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

	memmove(&v->cell[i], &v->cell[i+1],sizeof(lval*) * (v->count-i-1));
	v->count --;
	v-> cell = realloc(v->cell, sizeof(lval*) * v->count);
	return x;
}


lval* lval_take(lval* v,int i ){
	lval* x = lval_pop(v,i);
	lval_del(v);
	return x;
}


lval* lval_eval(lenv* env,lval* v){
	/**
	 * Evaluate an lval within the context of an enviroment.
	 *
	 * lenv* env: The enviroment to get variables from.
	 * lval* v: The lval to evaluate.
	 */
	if (v->type == LVAL_SYM){ 
		lval* x = lenv_get(env,v);
		lval_del(v); // delete the LVAL_SYM as its been replaced
		return x;
	}
	if (v->type == LVAL_SEXPR){
		return lval_eval_sexpr(env,v);
	}
	return v;}


lval* lval_eval_sexpr(lenv* env,lval* v){
	
	// eval all chilren
	for (int i = 0; i< v->count; i++){
		v->cell[i] = lval_eval(env,v->cell[i]);
//		printf("Evaluated : ");
//		lval_println(v->cell[i]);
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
	if (f->type != LVAL_FUN){
		lval* err  = lval_err(
				"S-Expression starts with the incorrect type"
				"Got %s but expected %s.",
				ltype_name(f->type),
				ltype_name(LVAL_FUN));
		lval_del(f);
		lval_del(v);
		return err;
	}
	
	// evaluate using builtin
	lval* result = lval_call(env,f,v);
	lval_del(f);
	return result;
}

int main(int argc, char** argv){
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* String = mpc_new("string");
	mpc_parser_t* Sexpr = mpc_new("sexpr");
	mpc_parser_t* Qexpr = mpc_new("qexpr");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Lispy = mpc_new("lispy");
	
	// Language definition
	mpca_lang(MPCA_LANG_DEFAULT,
			"\
				number: /-?[0-9]+/;\
				symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/;\
				string: /\"(\\\\.|[^\"])*\"/ ;\
				sexpr: '(' <expr>* ')';\
				qexpr: '{' <expr>* '}';\
				expr: <number> | <symbol> | <sexpr> | <qexpr> | <string>;\
				lispy: /^/ <expr>+ /$/;\
			",
			Number, Symbol, String, Sexpr, Qexpr, Expr, Lispy);

	puts("Lispy Version 0.0.0.1");
	puts("Press Ctrl+C to Exit \n");
	

	lenv* env = lenv_new();
	lenv_add_builtins(env);
	while(1){
		char* input = readline("lispy >");
		add_history(input);

		mpc_result_t r;
		if (mpc_parse("<stdin>",input,Lispy,&r) ){
			lval* x = lval_eval(env, lval_read(r.output));
			lval_println(x);
			lval_del(x);
			mpc_ast_delete(r.output);
		} else{
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input);
	}
	lenv_del(env);
	mpc_cleanup(6, Number, Symbol,String, Sexpr, Expr, Lispy);
	return 0;
}


