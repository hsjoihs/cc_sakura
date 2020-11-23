#include "cc_sakura.h"

int alloc_size;
Token *token;
Str *strings;
//LVar *locals;
//Func *func_list[100]; 

Node *data(){
	if(consume("(")){
		// jmp expr
		Node *node = expr();
		// check end of caret
		expect(")");
		return node;
	}

	// character literal
	if(consume("'")){
		Node *node=new_node_num((int)*(token->str));
		token=token->next;
		expect("'");
		return node;
	}

	// variable
	Token *tok = consume_ident();
	if(tok){
		Node *node = calloc(1, sizeof(Node));

		LVar *lvar = find_lvar(tok);
		if(lvar){
			node->kind   = (lvar->type->ty == ARRAY)? ND_LARRAY : ND_LVAR;
			node->offset = lvar->offset;
			node->type   = lvar->type;
		// call function
		}else if(check("(")){
			Func *called = find_func(tok);
			if(called){
				node->type = called->type;
			}

			node = call_function(node, tok);
		}else{
			GVar *gvar = find_gvar(tok);
			if(gvar){
				// global variable exist
				node->kind = (gvar->type->ty == ARRAY)? ND_GARRAY : ND_GVAR;
				node->type = gvar->type;
				node->str  = tok->str;
				node->val  = tok->len;
			}else{
				// variable does not exist.
				error_at(token->str, "this variable is not declaration");
			}
		}

		return node;
	}

	// return new num node
	return new_node_num(expect_number());
}

Node *primary(){
	Node *node = data();

	// Is array index
	while(consume("[")){
		node = array_index(node, mul());
		expect("]");
	}

	// increment
	if(consume("++")){
		node = incdec(node, POST_INC);
	}

	// decrement
	if(consume("--")){
		node = incdec(node, POST_DEC);
	}

	// member variable
	while(check(".") || check("->")){
		// dot
		if(consume(".")){
			if(node->kind == ND_LVAR){
				node = new_node(ND_ADDRESS, NULL, node);
			}
			node = dot_arrow(ND_DOT, node);
		}

		// arrow
		if(consume("->")){
			node = dot_arrow(ND_ARROW, node);
		}

		// array index
		while(consume("[")){
			node = array_index(node, mul());
			expect("]");
		}
	}

	return node;
}

Node *unary(){
	Node *node=NULL;

	if(consume("*")){
		node = new_node(ND_DEREF, NULL, unary());

		return node;
	}

	if(consume("&")){
		node = new_node(ND_ADDRESS, NULL, unary());
		node->type->ty    = PTR;
		node->type->size  = type_size(node->type);
		node->type->align = type_align(node->type);

		return node;
	}

	if(token->kind == TK_STR){
		consume("\"");
		Node *node = calloc(1, sizeof(Node));
		node->kind = ND_STR;
		node->type = calloc(1, sizeof(Type));
		node->type->ty = PTR;

		Token *tok = consume_string();
		Str *fstr = find_string(tok);

		// has already
		if(fstr){
			node->str = fstr->str;
			node->val = fstr->label_num;
			node->offset = fstr->len;
		// new one
		}else{
			Str *new = calloc(1, sizeof(Str));
			new->len = tok->len;
			new->str = tok->str;
			new->label_num = strings ? strings->label_num+1 : 0;
			node->str = new->str;
			node->offset = new->len;
			node->val = new->label_num;

			if(strings == NULL){
				strings = new;
			}else{
				new->next = strings;
				strings = new;
			}
		}

		return node;
	}

	if(consume("+")){
		//ignore +n
		return primary();
	}

	if(consume("-")){
		//convert to 0-n
		return new_node(ND_SUB, new_node_num(0), primary());
	}

	// increment
	if(consume("++")){
		return incdec(primary(), PRE_INC);
	}

	// decrement
	if(consume("--")){
		return incdec(primary(), PRE_DEC);
	}

	if(consume_reserved_word("sizeof", TK_SIZEOF)){
		// sizeof(5)  = > 4
		// sizeof(&a)  = > 8

		node = new_node(ND_NUM, node, unary());
		node->val = node->rhs->type->size;
		return node;
	}

	return primary();
}

Node *mul(){
	//jmp unary()
	Node *node = unary();

	for(;;){
		// is * and move the pointer next
		if(consume("*")){
			//create new node and jmp unary
			node = new_node(ND_MUL, node, unary());
		}else if(consume("/")){
			node = new_node(ND_DIV, node, unary());
		}else if(consume("%")){
			node = new_node(ND_MOD, node, unary());
		}else{
			return node;
		}
	}
}

Node *add(){
	//jmp mul()
	Node *node = mul();

	for(;;){
		if(consume("+")){
			node = new_node(ND_ADD, node, mul());
		}else if(consume("-")){
			node = new_node(ND_SUB, node, mul());
		}else{
			return node;
		}
	}
}

Node *relational(){
	Node *node = add();

	for(;;){
		//prefer multi symbol
		if(consume(">=")){
			node = new_node(ND_GE, node, add());
		}else if(consume("<=")){
			node = new_node(ND_LE, node, add());
		}else if(consume(">")){
			node = new_node(ND_GT, node, add());
		}else if(consume("<")){
			node = new_node(ND_LT, node, add());
		}else{
			return node;
		}
	}
}

Node *equelity(){
	Node *node = relational();

	for(;;){
		if(consume("==")){
			node = new_node(ND_EQ, node, relational());
		}else if(consume("!=")){
			node = new_node(ND_NE, node, relational());
		}else{
			return node;
		}
	}
}

Node *logical(){
	Node *node = equelity();
	for(;;){
		if(consume("&&")){
			node = new_node(ND_AND, node, equelity());
		}else if(consume("||")){
			node = new_node(ND_OR, node, equelity());
		}else{
			return node;
		}
	}
}

Node *ternary(){
	Node *node = logical();
	if(consume("?")){
		//                          cond  if true
		node = new_node(ND_TERNARY, node, ternary());
		expect(":");
		//             if false
		node->vector = ternary();
	}

	return node;
}

Node *assign(){
	Node *node = ternary();

	if(consume("=")){
		node = new_node(ND_ASSIGN, node, assign());
	}else if(consume("+=")){
		node = compound_assign(ND_ADD, node, assign());
	}else if(consume("-=")){
		node = compound_assign(ND_SUB, node, assign());
	}else if(consume("*=")){
		node = compound_assign(ND_MUL, node, assign());
	}else if(consume("/=")){
		node = compound_assign(ND_DIV, node, assign());
	}

	return node;
}

Node *expr(){
	int star_count = 0;
	Node *node;

	if(token->kind == TK_TYPE){
		node	   = calloc(1, sizeof(Node));
		node->kind = ND_LVAR;

		// parsing type
		node->type = parse_type();


		// variable declaration
		Token *tok = consume_ident();
		if(tok){
			node = declare_local_variable(node, tok, star_count);
		}else{
			error_at(token->str, "not a variable.");
		}

		// initialize formula
		if(consume("=")){
			if(consume("{")){
				node->vector = array_block(node);
			}else{
				node->vector = init_formula(node, assign());
			}
		}
	}else if(consume_reserved_word("break", TK_BREAK)){
		node	   = calloc(1, sizeof(Node));
		node->kind = ND_BREAK;
	}else if(consume_reserved_word("continue", TK_CONTINUE)){
		node	   = calloc(1, sizeof(Node));
		node->kind = ND_CONTINUE;
	}else{
		node = assign();
	}

	return node;
}

Node *stmt(){
	Node *node = NULL;

	if(consume_reserved_word("return", TK_RETURN)){
		node = new_node(ND_RETURN, node, expr());
		if(!consume(";")){
			error_at(token->str, "not a ';' token.");
		}
	}else if(consume_reserved_word("if", TK_IF)){
		/*
		 * =========== if =========== 
		 *
		 *     (cond)<--if-->expr
		 *
		 * ========= else if =========== 
		 *
		 *     (cond)<--if-----+
		 *                     | 
		 *        if(cond)<--else-->expr
		 */
		node = new_node(ND_IF, node, NULL);
		if(consume("(")){
			//jmp expr
			Node *cond = expr();
			//check end of caret
			expect(")");

			// (cond)<-if->expr
			node->lhs = cond;
			node->rhs = stmt();
		}

		if(consume_reserved_word("else", TK_ELSE)){
			// if()~ <-else-> expr
			Node *else_block = new_node(ND_ELSE, node, stmt());
			else_block->lhs  = node->rhs;
			node->rhs  = else_block;
			node->kind = ND_IFELSE;
		}
	}else if(consume_reserved_word("switch", TK_SWITCH)){
		/*
		 * default<--switch----+
		 *                     | (rhs)
		 *                     | 
		 *         (cond)<---case--->in_label(codes)
		 *                     |          | 
		 *                     |          | (vector: in_label)
		 *                     |          +----->code->code->... 
		 *                     | 
		 *                     | (next: chain_case)
		 *                     +----->case->case->... 
		 */

		Node *cond = NULL;
		node = new_node(ND_SWITCH, node, NULL);
		if(consume("(")){
			//jmp expr
			cond = expr();
			//check end of caret
			expect(")");
		}else{
			error_at(token->str, "expected ‘(’ before ‘{’ token");
		}

		Node *chain_case = NULL;
		expect("{");
		while(token->kind == TK_CASE || token->kind == TK_DEFAULT){
			if(consume_reserved_word("case", TK_CASE)){
				if(chain_case){
					chain_case->vector = new_node(ND_CASE, new_node(ND_EQ, cond, logical()), NULL);
					chain_case         = chain_case->vector;
				}else{
					chain_case   = new_node(ND_CASE, new_node(ND_EQ, cond, logical()), NULL);
					node->rhs    = chain_case;
				}
				expect(":");

				Node *in_label = NULL;
				while(token->kind != TK_CASE && token->kind != TK_DEFAULT){
					if(in_label){
						in_label->vector = stmt();
						in_label = in_label->vector;
					}else{
						in_label = stmt();
						chain_case->rhs = in_label;
					}

					if(check("}")) break;
				}
			}else if(consume_reserved_word("default", TK_DEFAULT)){
				expect(":");
				if(node->lhs == NULL){
					Node *in_label = NULL;
					node->lhs = new_node(ND_CASE, NULL, NULL);
					while(token->kind != TK_CASE && token->kind != TK_DEFAULT){
						if(in_label){
							in_label->vector = stmt();
							in_label = in_label->vector;
						}else{
							in_label       = new_node(ND_CASE, NULL, stmt());
							node->lhs->rhs = in_label;
						}

						if(check("}")) break;
					}
				}else{
					error_at(token->str, "multiple default labels in one switch");
				}
			}else{
				error_at(token->str, "statement will never be executed");
			}
		}
		expect("}");
	}else if(consume_reserved_word("for", TK_FOR)){
		node = new_node(ND_FOR, node, NULL);
		if(consume("(")){
			//jmp expr
			Node *init = expr();
			expect(";");
			Node *cond = expr();
			expect(";");
			Node *calc = expr();
			//check end of caret
			expect(")");

			// +-----------------------+
			// +-> (init->cond->loop)  +<-for->expr
			node->rhs = stmt();
			node->lhs = init;
			node->lhs->vector = cond;
			node->lhs->vector->vector = calc;
		}
	}else if(consume_reserved_word("while", TK_WHILE)){
		node = new_node(ND_WHILE, node, NULL);
		if(consume("(")){
			//jmp expr
			Node *cond = expr();
			//check end of caret
			expect(")");

			// (cond)<-while->expr
			node->lhs = cond;
			node->rhs = stmt();
		}
	}else if(consume("{")){
		node = new_node(ND_BLOCK, node, NULL);

		Node *block_code = calloc(1, sizeof(Node));
		while(token->kind!=TK_BLOCK){
			//Is first?
			if(block_code->rhs){
				block_code->vector = stmt();
				block_code = block_code->vector;
			}else{
				block_code = stmt();
				node->vector = block_code;
			}
		}
		expect("}");
	}else{
		node = expr();
		if(!consume(";")){
			while(*(token->str)!='\n') (token->str)--;
			error_at(token->str, "not a ';' token.");
		}
	}

	return node;
}

void function(Func *func){
	int i = 0;

	// while end of function block
	while(!consume("}")){
		func->code[i++] = stmt();
	}

	func->stack_size = alloc_size;
	func->code[i] = NULL;
}

void program(){
	int func_index = 0;
	int star_count;
	Type *toplv_type;

	while(!at_eof()){
		// reset lvar list
		locals = NULL;
		// reset lvar counter
		alloc_size = 0;
		star_count = 0;
		func_list[func_index] = (Func *)malloc(sizeof(Func));

		toplv_type = calloc(1,sizeof(Type));

		// type of function return value
		if(token->kind == TK_TYPE){
			if(consume_reserved_word("int", TK_TYPE)){
				toplv_type->ty = INT;
			}else if(consume_reserved_word("char", TK_TYPE)){
				toplv_type->ty = CHAR;
			}else if(consume_reserved_word("struct", TK_TYPE)){
				toplv_type->ty = STRUCT;
			}
		}

		// count asterisk
		while(token->kind == TK_RESERVED && *(token->str) == '*'){
			star_count++;
			token = token->next;
		}


		// Is function?
		if(token->kind != TK_IDENT ||!(is_alnum(*token->str))){
			error_at(token->str, "not a function.");
		}

		Token *def_name = consume_ident();

		// function
		if(consume("(")){
			func_list[func_index]->type = toplv_type;
			func_list[func_index]->name = calloc(def_name->len, sizeof(char));
			strncpy(func_list[func_index]->name, def_name->str, def_name->len);
			
			// add type list
			func_list[func_index]->type = insert_type_list(func_list[func_index]->type, star_count);

			// get arguments
			get_argument(func_index);

			// get function block
			consume("{");
			function(func_list[func_index++]);
			consume("}");
		// struct
		}else if(consume("{")){
			if(toplv_type->ty != STRUCT){
				error_at(token->str, "not a struct.");
			}

			Struc *new_struc = calloc(1,sizeof(Struc));
			new_struc->len   = def_name->len;
			new_struc->name  = def_name->str;

			declare_struct(new_struc);

			expect(";");
		// global variable
		}else{
			Node *init_gv = declare_global_variable(star_count, def_name, toplv_type);

			// initialize formula
			if(consume("=")){
				if(consume("{")){
					globals->init = array_block(init_gv);
				}else{
					globals->init = init_formula(init_gv, assign());
				}
			}else{
				if(init_gv->kind == ND_GVAR){
					globals->init = init_formula(init_gv, new_node_num(0));
				}
			}

			expect(";");
		}
	}
	func_list[func_index] = NULL;
}
