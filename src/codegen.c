#include "cc_sakura.h"

// int label_num;
// int label_ctr;

void expand_next(Node *node){
	while(node){
		gen(node);
		printf("	pop rax\n");
		node=node->next;
	}
	printf("	push rax\n");
}

void expand_vector(Node *node){
	while(node){
		gen(node);
		printf("	pop rax\n");
		node=node->vector;
	}
}


void gen_gvar(Node *node){
	printf("	lea rax,  _%.*s[rip]\n", node->val, node->str);
	printf("	push rax\n");
}

void gen_lvar(Node *node){
	if(node->kind != ND_LVAR && node->kind != ND_LARRAY && node->kind != ND_CALL_FUNC){
		error_at(token->str,"not a variable");
	}

	printf("	mov rax,rbp\n");
	printf("	sub rax,%d\n", node->offset);
	printf("	push rax\n");
}

void gen_struc(Node *node){
	if(node->kind != ND_DOT && node->kind != ND_ARROW){
		error_at(token->str, "not a struct");
	}

	gen(node->lhs);
	gen(node->rhs);

	printf("	pop rdi\n");
	printf("	pop rax\n");
	printf("	add rax,rdi\n");
	printf("	push rax\n");
}

void gen_arg(int arg_num, Node *tmp){
	const char reg[6][4]={"rdi","rsi","rdx","rcx","r8","r9"};

	gen(tmp);
	printf("	pop rax\n");
	printf("	mov %s,rax\n", reg[arg_num]);
}

void gen_address(Node *node){
	/**/ if(node->kind == ND_DEREF)   gen(node->rhs);
	else if(node->kind == ND_DOT)     gen_struc(node);
	else if(node->kind == ND_ARROW)   gen_struc(node);
	else if(node->kind == ND_GVAR)    gen_gvar(node);
	else if(node->kind == ND_GARRAY)  gen_gvar(node);
	else if(node->kind == ND_LVAR)    gen_lvar(node);
	else if(node->kind == ND_LARRAY)  gen_lvar(node);
	else error_at(token->str, "can not assign");
}

void gen_calc(Node *node){
	const char reg_ax[4][4]={"eax","eax","rax","rax"};
	const char reg_dx[4][4]={"edx","edx","rdx","rdx"};
	const char reg_di[4][4]={"edi","edi","rdi","rdi"};
	int reg_ty = (int)node->type->ty;

	switch(node->kind){
		case ND_ADD:
			printf("	add %s,%s\n", reg_ax[reg_ty], reg_di[reg_ty]);
			break;
		case ND_SUB:
			printf("	sub %s,%s\n", reg_ax[reg_ty], reg_di[reg_ty]);
			break;
		case ND_MUL:
			printf("	imul %s,%s\n", reg_ax[reg_ty], reg_di[reg_ty]);
			break;
		case ND_DIV:
			printf("	cqo\n");
			printf("	idiv %s,%s\n", reg_ax[reg_ty], reg_di[reg_ty]);
			break;
		case ND_MOD:
			printf("	cqo\n");
			printf("	idiv %s,%s\n", reg_ax[reg_ty], reg_di[reg_ty]);
			printf("	mov %s,%s\n", reg_ax[reg_ty], reg_dx[reg_ty]);
			break;
		case ND_GT:
			printf("	cmp %s,%s\n", reg_di[reg_ty], reg_ax[reg_ty]);
			printf("	setl al\n");
			printf("	movzb rax,al\n");
			break;
		case ND_GE:
			printf("	cmp %s,%s\n", reg_di[reg_ty], reg_ax[reg_ty]);
			printf("	setle al\n");
			printf("	movzb rax,al\n");
			break;
		case ND_LT:
			printf("	cmp %s,%s\n", reg_ax[reg_ty], reg_di[reg_ty]);
			printf("	setl al\n");
			printf("	movzb rax,al\n");
			break;
		case ND_LE:
			printf("	cmp %s,%s\n", reg_ax[reg_ty], reg_di[reg_ty]);
			printf("	setle al\n");
			printf("	movzb rax,al\n");
			break;
		case ND_EQ:
			printf("	cmp %s,%s\n", reg_ax[reg_ty], reg_di[reg_ty]);
			printf("	sete al\n");
			printf("	movzb rax,al\n");
			break;
		case ND_NE:
			printf("	cmp %s,%s\n", reg_ax[reg_ty], reg_di[reg_ty]);
			printf("	setne al\n");
			printf("	movzb rax,al\n");
			break;
		case ND_AND:
			printf("	and %s,%s\n", reg_ax[reg_ty], reg_di[reg_ty]);
			printf("	movzb rax,al\n");
			break;
		case ND_OR:
			printf("	or %s,%s\n", reg_ax[reg_ty], reg_di[reg_ty]);
			printf("	movzb rax,al\n");
			break;
	}
}

void gen(Node *node){
	Node *tmp;
	int arg=0;

	switch(node->kind){
		case ND_NUM:
			printf("	push %d\n", node->val);
			return;
		case ND_GVAR:
			gen_gvar(node);

			printf("	pop rax\n");
			printf("	mov rax,[rax]\n");
			printf("	push rax\n");

			return;
		case ND_LVAR:
			gen_lvar(node);

			printf("	pop rax\n");
			if(node->type->ty == CHAR){
				printf("	movzx eax,BYTE PTR [rax]\n");
				printf("	movsx eax,al\n");
			}else{
				printf("	mov rax,[rax]\n");
			}
			printf("	push rax\n");

			// init formula
			if(node->vector != NULL) gen(node->vector);
			return;
		case ND_GARRAY:
			gen_gvar(node);

			// init formula
			if(node->vector != NULL) expand_next(node->vector);
			return;
		case ND_LARRAY:
			gen_lvar(node);

			// init formula
			if(node->vector != NULL) expand_next(node->vector);
			return;
		case ND_PREID:
			//gen(node->lhs);
			//printf("	pop rax\n");

			// ++p -> p += 1
			gen(node->lhs);
			return;
		case ND_POSTID:
			// push
			gen_address(node->lhs); // push lhs
			gen(node->rhs->rhs->rhs);          // push rhs
			
			// calc
			printf("	pop rdi\n");    // rhs
			printf("	pop rax\n");    // lhs
			printf("	push [rax]\n"); // Evacuation lhs data
			printf("	push rax\n");   // Evacuation lhs address
			printf("	mov rax,[rax]\n"); // deref lhs

			gen_calc(node->rhs->rhs);
			printf("	push rax\n"); // rhs op lhs

			// assign
			printf("	pop rdi\n"); // src
			printf("	pop rax\n"); // dst
			if(node->lhs->type->ty == CHAR){
				printf("	mov [rax],dil\n");
			}else if(node->lhs->type->ty == INT){
				printf("	mov [rax],edi\n");
			}else{
				printf("	mov [rax],rdi\n");
			}

			return;
		case ND_STR:
			printf("	lea rax, .LC%d[rip]\n", node->val);
			printf("	push rax\n");
			return;
		case ND_ASSIGN:
			gen_address(node->lhs);
			gen(node->rhs);

			printf("	pop rdi\n");
			printf("	pop rax\n");
			if(node->lhs->type->ty == CHAR){
				printf("	mov [rax],dil\n");
			}else if(node->lhs->type->ty == INT){
				printf("	mov [rax],edi\n");
			}else{
				printf("	mov [rax],rdi\n");
			}

			printf("	push rdi\n");

			return;
		case ND_COMPOUND:
			// push
			gen_address(node->lhs); // push lhs
			gen(node->rhs->rhs);  // push rhs

			// calc
			printf("	pop rdi\n");  // rhs
			printf("	pop rax\n");  // lhs
			printf("	push rax\n"); // Evacuation lhs
			printf("	mov rax,[rax]\n"); // deref lhs

			gen_calc(node->rhs);
			printf("	push rax\n"); // rhs op lhs

			// assign
			printf("	pop rdi\n"); // src
			printf("	pop rax\n"); // dst
			if(node->lhs->type->ty == CHAR){
				printf("	mov [rax],dil\n");
			}else if(node->lhs->type->ty == INT){
				printf("	mov [rax],edi\n");
			}else{
				printf("	mov [rax],rdi\n");
			}

			printf("	push rdi\n");

			return;
		case ND_DOT:
		case ND_ARROW:
			gen_struc(node);
			// if it's an array, ignore the deref
			if(node->type->ty != ARRAY){
				printf("	pop rax\n");
				printf("	push [rax]\n");
			}
			return;
		case ND_IF:
			label_num++;
			label_depth++;

			printf("	push rax\n");
			gen(node->lhs);

			printf("	pop rax\n");
			printf("	cmp rax,0\n");
			printf("	je .Lend%03d\n", label_num);
			printf("	pop rax\n");
			gen(node->rhs);

			printf(".Lend%03d:\n", label_num);
			label_num--;
			return;
		case ND_IFELSE:
			label_num++;
			label_depth++;

			// condition
			gen(node->lhs);
			printf("	pop rax\n");
			printf("	cmp rax,0\n");
			printf("	je .Lelse%03d\n", label_num);

			// expr in if
			gen(node->rhs->lhs);
			printf("	jmp .Lend%03d\n", label_num);
			printf(".Lelse%03d:\n", label_num);

			// expr in else
			gen(node->rhs->rhs);
			printf(".Lend%03d:\n", label_num);

			label_num--;
			return;
		case ND_FOR:
			// adjust rsp
			printf("	push rax\n");

			// init
			gen(node->lhs);

			// condition
			label_num++;
			label_depth++;

			printf(".Lbegin%03d:\n", label_num);
			gen(node->lhs->vector);
			printf("	pop rax\n");
			printf("	cmp rax,0\n");
			// if cond true then jump to  loop end.
			printf("	je .Lend%03d\n", label_num);

			// else expression
			gen(node->rhs);
			gen(node->lhs->vector->vector);
			printf("	pop rax\n");

			// continue
			printf("	jmp .Lbegin%03d\n", label_num);
			printf(".Lend%03d:\n", label_num);
			printf("	push rax\n");

			label_num--;
			return;
		case ND_WHILE:
			label_num++;
			label_depth++;

			// adjust rsp
			printf("	push rax\n");

			// condition
			printf(".Lbegin%03d:\n", label_num);
			gen(node->lhs);
			printf("	pop rax\n");
			printf("	cmp rax,0\n");
			// if cond true then loop end.
			printf("	je .Lend%03d\n", label_num);

			// else expression
			gen(node->rhs);

			// continue
			printf("	jmp .Lbegin%03d\n", label_num);
			printf(".Lend%03d:\n", label_num);
			printf("	push rax\n");

			label_num--;
			return;
		case ND_BREAK:
			printf("	pop rax\n");
		case ND_BLOCK:
			printf("	jmp .Lend%03d\n", label_num);
			return;
		case ND_CALL_FUNC:
			tmp=node->next;
			arg=node->val;

			if(tmp!=NULL){
				while(tmp->next!=NULL){
					gen_arg(arg, tmp);
					tmp=tmp->next;
					arg--;
				}
				gen_arg(arg, tmp);
			}

			printf("	push rbp\n");
			printf("	mov rbp,rsp\n");
			printf("	and rsp,-16\n");

			printf("	call %s\n", node->str);

			printf("	mov rsp,rbp\n");
			printf("	pop rbp\n");

			printf("	push rax\n");
			return;
		case ND_ARG:
			tmp = node;
			while(tmp){
				// generate arg as lvar
				gen(tmp->next);
				printf("	pop rax\n");
				gen_lvar(tmp->next);
				printf("	pop rax\n");
				printf("	pop rdi\n");
				printf("	mov [rax],rdi\n");
				printf("	push rdi\n");
				tmp=tmp->rhs;
				// pop stack top
				printf("	pop rax\n");
			}

			return;
		case ND_ADDRESS:
			gen_address(node->rhs);
			return;
		case ND_DEREF:
			gen(node->rhs);
			printf("	pop rax\n");
			if(node->type->ty == CHAR){
				printf("	movzx eax,BYTE PTR [rax]\n");
				printf("	movsx eax,al\n");
			}else{
				printf("	mov rax,[rax]\n");
			}
			printf("	push rax\n");

			return;
		case ND_RETURN:
			gen(node->rhs);
			printf("	pop rax\n");
			printf("	mov rsp,rbp\n");
			printf("	pop rbp\n");
			printf("	ret\n");
			return;
	}


	// check left hand side
	gen(node->lhs);
	// check right hand side
	gen(node->rhs);

	// pop two value
	printf("	pop rdi\n");
	printf("	pop rax\n");
	// calculation lhs and rhs
	gen_calc(node);
	// push result
	printf("	push rax\n");
}

