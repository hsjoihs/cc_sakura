#include "cc_sakura.h"

int type_size(Type *type){
	switch(type->ty){
		case VOID:
			return 1;
		case CHAR:
			return 1;
		case INT:
			return 4;
		case PTR:
			return 8;
		case ARRAY:
			return type->index_size * type_size(type->ptr_to);
		case STRUCT:
			return type->size;
		case ENUM:
			return 4;
		default:
			error_at(token->str, "unknown type");
	}

	return -1;
}

int type_align(Type *type){
	int max=0;
	int align_num;
	Member *memb_list;

	switch(type->ty){
		case VOID:
			return 1;
		case CHAR:
			return 1;
		case INT:
			return 4;
		case PTR:
			return 8;
		case ARRAY:
			return type_align(type->ptr_to);
		case STRUCT:
			memb_list = type->member;
			while(memb_list){
				align_num = type_align(memb_list->type);
				if(max < align_num){
					max = align_num;
				}
				memb_list = memb_list->next;
			}
			return max;
		case ENUM:
			return 4;
		default:
			error_at(token->str, "unknown type");
	}

	return -1;
}

int align_array_size(int isize, Type *array_type){
	int array_size = array_type->size;
	return (array_size%8) ? array_size/8*8+8 : array_size;
}


Type *get_pointer_type(Type *given){
	while(given->ptr_to != NULL) given = given->ptr_to;
	return given;
}

Node *pointer_calc(Node *node, Type *lhs_type, Type *rhs_type){
	Type *ptrtype;


	node->type = (lhs_type->ty > rhs_type->ty)? lhs_type : rhs_type;
	Node *pointer_size = calloc(1, sizeof(Node));
	pointer_size->kind = ND_NUM;
	pointer_size->type = calloc(1, sizeof(Type));
	pointer_size->type->ty    = INT;
	pointer_size->type->size  = type_size(pointer_size->type);
	pointer_size->type->align = type_align(pointer_size->type);


	if(lhs_type->ty >= PTR  &&  lhs_type->ptr_to!=NULL){
		ptrtype = lhs_type->ptr_to;
		pointer_size->val = ptrtype->size;
		node->rhs = new_node(ND_MUL, node->rhs, pointer_size);
	}else if(rhs_type->ty >= PTR  &&  rhs_type->ptr_to!=NULL){
		ptrtype = rhs_type->ptr_to;
		pointer_size->val = ptrtype->size;
		node->lhs = new_node(ND_MUL, node->lhs, pointer_size);
	}

	return node;
}
