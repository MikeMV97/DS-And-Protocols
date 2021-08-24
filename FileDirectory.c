#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LEN_NAME 15

typedef struct _node{
	char 			file_name[LEN_NAME];
	struct _node*	parent;
	struct _node* 	children;
	struct _node*	next_sibling;
	struct _node*	prev_sibling;
} Node;
typedef Node * Node_ptr;

//--------------------------------------------------------
char *read_name(){
    static char cr[LEN_NAME] = "000000000000000";
    fflush(stdin);
	printf("Nombre: ");
    fflush(stdin);
	scanf("%s", cr);// fgets(cr, LEN_NAME, stdin);// gets(cr);
    return cr;
}

//--------------------------------------------------------
Node_ptr create_node(char dat[]){
    Node_ptr node = (Node_ptr) malloc(sizeof(Node));

	node->parent = NULL;
	node->next_sibling = NULL;
	node->prev_sibling = NULL;
	node->children = NULL;
	strcpy(node->file_name, dat);

    return node;
}

Node_ptr insert_node(Node_ptr root){
	Node_ptr nd = create_node(read_name());
	nd->parent = root;

	if (root->children == NULL){
		root->children = nd;
	} 
	else {
		Node_ptr aux = root->children;
		while( aux->next_sibling != NULL ){
			aux = aux->next_sibling;
		}
		aux->next_sibling = nd;
		nd->prev_sibling = aux;
	}

	return root;
}

//-------------------------------------------------------
Node_ptr seek_file(char name[], Node_ptr root){
	if(root == NULL)
		return NULL;
	if(strcmp(name, root->file_name) == 0)
		return root;

	Node_ptr child = root->children;
	Node_ptr res = NULL;
	while (child != NULL){
		res = seek_file(name, child);
		if(res != NULL)
			return res;
		child = child->next_sibling;
	}
	
	return NULL;
}

//--------------------------------------------------------
Node_ptr create_file(Node_ptr root){

	if (root == NULL){	
		printf("Raiz, ");
		return create_node(read_name());
	}
	
	printf("Archivo padre, ");
	Node_ptr parent_file = seek_file(read_name(), root);
	if(parent_file != NULL){
		parent_file = insert_node(parent_file);
	} else{
		printf("Disculpe, imposible encontrar archivo.\n");
	}
	return root;
}


//---------------------------------------------------------
void delete_all(Node_ptr root){ //	Retorna al child
	if ( root == NULL)
		return;

	if ( root->children != NULL ){
		delete_all(root->children);
	}
	if ( root->next_sibling != NULL ){
		delete_all(root->next_sibling);
	}
	root->parent = NULL;
	root->prev_sibling = NULL;
	free(root);
}

Node_ptr delete_file(Node_ptr root){
	Node_ptr file_to_delete = seek_file(read_name(), root);
	if(file_to_delete == root){
		delete_all(root);
		return NULL;
	}

	if (file_to_delete != NULL){
		if(file_to_delete->prev_sibling != NULL){
			file_to_delete->prev_sibling->next_sibling = file_to_delete->next_sibling;
		} 
		else if(file_to_delete->parent != NULL){
			// First child, Or the Root
			file_to_delete->parent->children = file_to_delete->next_sibling;
		}
		file_to_delete->next_sibling = NULL;
		file_to_delete->prev_sibling = NULL;
		file_to_delete->parent = NULL;
		delete_all(file_to_delete);
	} else {
		printf("No existe esa file.\n");
	}
	return root;
}
//----------------------------------------------------------
void print_files(Node_ptr root, int LVL){
    int j;
	if (root != NULL) {
        printf("%s\n", root->file_name);
		Node_ptr child = root->children;
        while (child != NULL){
        	for (j = 0 ; j < LVL ; ++j) printf("   ");
        	print_files(child, LVL+1);
			child = child->next_sibling;
		}
     }
}
//----------------------------------------------------------
//----------------------------------------------------------
int main(int argc, char *argv[]) {
	Node_ptr nArbol= NULL;
    char msj;
	char input[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int i, opc_invalida = 1;

    do{
        // system("cls");
		fflush(stdin);
		printf("\n\t1) Insertar\n");
		printf("\t2) Imprimir\n");
		printf("\t3) Borrar\n");
		printf("\ts) Salir\n");
		printf("Seleccione opcion: ");
		fflush(stdin); scanf("%s", input);

	   	msj = input[0];
		switch(msj){
			case '1':
				nArbol = create_file(nArbol);
				opc_invalida = 0;
				break;
			case '2':
				print_files(nArbol, 1);
				opc_invalida = 0;
				break;
			case '3': 
				nArbol = delete_file(nArbol);
				opc_invalida = 0;
				break;
			case 's':
				return 0;
				break;
			default:
				printf("No valida.\n");
				break;
		}
		opc_invalida = 1;
        // system("PAUSE");
    }while(msj != 's');
	
	return 0;
}
