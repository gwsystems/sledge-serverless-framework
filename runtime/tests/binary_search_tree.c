#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#define MAX_NODES 4096 // Maximum number of nodes in the pool

// Define the colors for Red-Black Tree
typedef enum { RED, BLACK } Color;

// Definition of a binary search tree node
struct TreeNode {
    struct TreeNode *left;
    struct TreeNode *right;
    struct TreeNode *next;  // pointing to the next node, this is used for nodePool 
                            // to find next available node
    struct TreeNode *dup_next; // pointing to next duplicate key item
    struct TreeNode *parent;
    Color           color;
    uint64_t        left_subtree_sum;
    uint64_t	    deadline;
    uint64_t        exe_time;
};

// Definition of TreeNode memory pool
struct TreeNodePool {
    struct TreeNode* head;
};

struct binary_tree {
	struct TreeNode 	            *root;
	struct TreeNodePool                 nodePool;
	int                                 id;
        int                                 queue_length;
};

// Initialize the node pool
void initNodePool(struct TreeNodePool *nodePool, int pool_size) {
    
    assert(nodePool != NULL);

    struct TreeNode *nodes = (struct TreeNode*)malloc(pool_size * sizeof(struct TreeNode));
    nodePool->head = nodes;  // Initialize head to the beginning of the node array
 
    for (int i = 0; i < MAX_NODES - 1; ++i) {
        nodes[i].next  = &nodes[i + 1];  // Set the next pointer of each node to the next node
        nodes[i].left  = NULL;
        nodes[i].right = NULL;
        nodes[i].dup_next = NULL;
	nodes[i].parent = NULL;
	nodes[i].color = RED; 
        nodes[i].left_subtree_sum = 0;
        nodes[i].deadline = 0;
        nodes[i].exe_time = 0;
    }
    nodes[MAX_NODES - 1].next = NULL;
}

struct binary_tree * init_binary_tree(int id, int queue_size) {
	
	struct binary_tree *binary_tree = (struct binary_tree *)calloc(1, sizeof(struct binary_tree));
	initNodePool(&binary_tree->nodePool, queue_size);
	binary_tree->root = NULL;
	binary_tree->id = id;
        binary_tree->queue_length = 0;
	

	return binary_tree;
}

// Function to get the total number of non-deleted nodes in the binary tree
int getNonDeletedNodeCount(struct binary_tree *binary_tree) {
    assert(binary_tree != NULL);
    return binary_tree->queue_length;
}

// Get a new node from the pool
struct TreeNode* newNode(struct binary_tree *binary_tree, Color color, struct TreeNode* left, 
			 struct TreeNode* right, struct TreeNode* parent, uint64_t deadline, uint64_t exe_time) {
    
    assert(binary_tree != NULL);

    if (binary_tree->nodePool.head == NULL) {
        printf("Binary search tree queue %d is full\n", binary_tree->id);
        return NULL;
    } else {
        // Remove a node from the head of the memory pool
        struct TreeNode* new_node_t = binary_tree->nodePool.head;
        binary_tree->nodePool.head = new_node_t->next;
        new_node_t->next = NULL;  // Reset the next pointer of the new node
        new_node_t->deadline = deadline;
        new_node_t->exe_time = exe_time;
        new_node_t->left = left; 
        new_node_t->right = right;
	new_node_t->parent = parent;
 	new_node_t->color = color;
        new_node_t->dup_next = NULL;
        new_node_t->left_subtree_sum = 0;
        return new_node_t;
    }
}

void getAvailableCapacity(struct binary_tree *binary_tree) {
	
    assert(binary_tree != NULL);
        
    int size = 0;
    struct TreeNode* start = binary_tree->nodePool.head;
    while(start) {
        size++;
        start = start->next;
    }

    printf("available capacity of the queue is %d\n", size);
}

void print_in_order(struct TreeNode* node) {
    if (node != NULL) {
        // Recursively traverse the left subtree
        print_in_order(node->left);
        
        // Print the data in the current node
        printf("%lu(%lu) ", node->deadline, node->exe_time);
        //Print data in the dup_next list
        struct TreeNode* cur = node->dup_next;
        while(cur) {
           printf("%lu(%lu) ", cur->deadline, cur->exe_time);
           cur = cur->dup_next;
        }

        // Recursively traverse the right subtree
        print_in_order(node->right);
    }
}

// Function to print the items in the binary search tree in order
void print_tree_in_order(struct binary_tree* bst) {
    if (bst != NULL) {
        print_in_order(bst->root);
        printf("\n");
    }
}

//get the total execute time of a node.
uint64_t get_sum_exe_time_of_node(struct TreeNode* node){
    uint64_t total = 0;
    struct TreeNode* curNode = node;
    while (curNode!=NULL) {
        total += curNode->exe_time;
        curNode = curNode->dup_next;
    }

    return total;
}

//get the total execute time of a node's subtree, including left and right subtree
uint64_t get_sum_exe_time_of_subtree(struct TreeNode* root) {
    if (root == NULL) {
	return 0;
    }
    
    return get_sum_exe_time_of_node(root) + root->left_subtree_sum + get_sum_exe_time_of_subtree(root->right);
}

// Return a node to the pool
void deleteNode(struct binary_tree *binary_tree, struct TreeNode* node) {

    assert(binary_tree != NULL);
    assert(node != NULL);

    // Insert the node back to the head of the memory pool
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->color = RED;
    node->deadline = 0;
    node->exe_time = 0;
    node->left_subtree_sum = 0;
    node->dup_next = NULL;
    node->next = binary_tree->nodePool.head;
    binary_tree->nodePool.head = node;
}

int findHeight(struct TreeNode *root)
{
    int lefth, righth;
    if(root == NULL)
        return 0;
    lefth = findHeight(root->left);
    righth = findHeight(root->right);
    return (lefth > righth ? lefth : righth)+1;
}
// Update root if rotating. 
void leftRotate(struct binary_tree *binary_tree, struct TreeNode *x) {
    struct TreeNode *y = x->right;
    x->right = y->left;
    if (y->left != NULL) 
	y->left->parent = x;

    y->parent = x->parent;

    if (x->parent == NULL) 
	binary_tree->root = y;
    else if (x == x->parent->left) 
	x->parent->left = y;
    else 
	x->parent->right = y;
    y->left = x;
    x->parent = y;
    y->left_subtree_sum += x->exe_time + x->left_subtree_sum;
}

// Update root if rotatiing
void rightRotate(struct binary_tree *binary_tree, struct TreeNode *x) {
    struct TreeNode *y = x->left;
    x->left = y->right;

    int new_sum_left = 0;
    if (y->right != NULL) {
        y->right->parent = x;
        new_sum_left = get_sum_exe_time_of_subtree(y->right); //y->right->exe_time + y->right->sum_left;
    }

    x->left_subtree_sum = new_sum_left;
    y->parent = x->parent;
    if (x->parent == NULL) 
	binary_tree->root = y;
    else if (x == x->parent->right) 
	x->parent->right = y;
    else 
	x->parent->left = y;
    y->right = x;
    x->parent = y;
}

void insertFixup(struct binary_tree *binary_tree, struct TreeNode *z) {
    while (z->parent != NULL && z->parent->color == RED) {
        if (z->parent == z->parent->parent->left) {
            struct TreeNode *y = z->parent->parent->right;
            if (y != NULL && y->color == RED) {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    z = z->parent;
                    leftRotate(binary_tree, z);
                }
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                rightRotate(binary_tree, z->parent->parent);
            }
        } else {
            struct TreeNode *y = z->parent->parent->left;
            if (y != NULL && y->color == RED) {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rightRotate(binary_tree, z);
                }
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                leftRotate(binary_tree, z->parent->parent);
            }
        }
    }
    binary_tree->root->color = BLACK;
}

// Function to insert a value into a binary search tree. Tree root might be changed after inserting a new node
void insert(struct binary_tree *binary_tree, uint64_t deadline, uint64_t exe_time) {
    struct TreeNode *z = newNode(binary_tree, RED, NULL, NULL, NULL, deadline, exe_time);
    binary_tree->queue_length++;
    struct TreeNode *y = NULL;
    struct TreeNode *x = binary_tree->root;
    while (x != NULL) {
        y = x;

        //if found a dup_next deadline, inserted the node
        //at the end of the linklist
        if (z->deadline == x->deadline) {
            //find the tail the link list;
            struct TreeNode* tail = x;
            while (tail->dup_next != NULL) {
                tail = tail->dup_next;
            }
            //append the new node at the end of the list;
            tail->dup_next = z;
            z->color = x->color;
            z->left_subtree_sum = x->left_subtree_sum;
            return;
        } else if (z->deadline < x->deadline) {
            x->left_subtree_sum += z->exe_time;
            x = x->left;
        } else {
            x = x->right;
        }
    }

    z->parent = y;
    if (y == NULL) {
        binary_tree->root = z;
    } else if (z->deadline < y->deadline) {
        y->left = z;
    } else {
        y->right = z;
    }

    insertFixup(binary_tree, z);
}

// Helper function to find the minimum value in a binary search tree
struct TreeNode* findMin(struct binary_tree *binary_tree) {
    
    assert(binary_tree != NULL);

    struct TreeNode *curNode = binary_tree->root;
    if (curNode == NULL) {
        return NULL;
    }

    while (curNode->left != NULL) {
        curNode = curNode->left; // Keep traversing to the left until the leftmost node is reached
    }
    return curNode;
}

// Helper function to find the maximum value in a binary search tree
struct TreeNode* findMax(struct binary_tree *binary_tree) {

    assert(binary_tree != NULL);
    
    struct TreeNode *curNode = binary_tree->root;
    if (curNode == NULL) {
        return NULL;
    }

    while (curNode->right != NULL) {
        curNode = curNode->right; // Keep traversing to the right until the rightmost node is reached
    }
    return curNode;
}

struct TreeNode* searchByKey(struct binary_tree *binary_tree, uint64_t deadline) {
    struct TreeNode* current = binary_tree->root;
    while (current != NULL && current->deadline != deadline) {
        if (deadline < current->deadline) {
            current = current->left;
        }
        else {
            current = current->right;
        }
    }

    return current;
}

void transplant(struct binary_tree *binary_tree, struct TreeNode *u, struct TreeNode *v) {
    if (u->parent == NULL) 
	binary_tree->root = v;
    else if (u == u->parent->left) 
	u->parent->left = v;
    else 
	u->parent->right = v;
    if (v != NULL) 
	v->parent = u->parent;
}

struct TreeNode* minimum(struct TreeNode *node) {
    while (node->left != NULL) {
        node = node->left;
    }
    return node;
}

void deleteFixup(struct binary_tree *binary_tree, struct TreeNode *x) {
    while (x != binary_tree->root && (x == NULL || x->color == BLACK)) {

        if (x == NULL){
            break;
        }

        if ( x == x->parent->left) {
            struct TreeNode *w = x->parent->right;
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                leftRotate(binary_tree, x->parent);
                w = x->parent->right;
            }
            if ((w->left == NULL || w->left->color == BLACK) && (w->right == NULL || w->right->color == BLACK)) {
                w->color = RED;
                x = x->parent;
            } else {
                if (w->right == NULL || w->right->color == BLACK) {
                    if (w->left != NULL) w->left->color = BLACK;
                    w->color = RED;
                    rightRotate(binary_tree, w);
                    w = x->parent->right;
                }
                w->color = x->parent->color;
                x->parent->color = BLACK;
                if (w->right != NULL) w->right->color = BLACK;
                leftRotate(binary_tree, x->parent);
                x = binary_tree->root;
            }
        } else {
            struct TreeNode *w = x->parent->left;
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                rightRotate(binary_tree, x->parent);
                w = x->parent->left;
            }
            if ((w->right == NULL || w->right->color == BLACK) && (w->left == NULL || w->left->color == BLACK)) {
                w->color = RED;
                x = x->parent;
            } else {
                if (w->left == NULL || w->left->color == BLACK) {
                    if (w->right != NULL) w->right->color = BLACK;
                    w->color = RED;
                    leftRotate(binary_tree, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
		x->parent->color = BLACK;
                if (w->left != NULL) w->left->color = BLACK;
                rightRotate(binary_tree, x->parent);
                x = binary_tree->root;
            }
        }
    }
    if (x != NULL) x->color = BLACK;
}

void removeNode(struct binary_tree *binary_tree, struct TreeNode *z) {
    struct TreeNode *y = z;
    struct TreeNode *x = NULL;
    Color y_original_color = y->color;

    if ( z->left != NULL && z->right != NULL ) {
        y = minimum(z->right);

        int diff = get_sum_exe_time_of_node(y) - get_sum_exe_time_of_node(z);
        struct TreeNode* cur = z->right;
        while (cur != y) {
            cur->left_subtree_sum -= diff;
            cur = cur->left;
        }

	uint64_t remove_deadline = z->deadline;
        uint64_t remove_exe_time = z->exe_time;
	z->deadline = y->deadline;
	z->exe_time = y->exe_time;
	z->dup_next = y->dup_next;
	y->deadline = remove_deadline;
	y->exe_time = remove_exe_time;
	y->dup_next = NULL;

        removeNode(binary_tree, y);
        return;
    }

    //now the node to be removed has only no children
    //or only one child, update the sum_left value of
    //all the node along the path from root to z.
    struct TreeNode* current = z;
    struct TreeNode* p = current->parent;
    while (p != NULL) {
        if (p->left == current)
        {
            p->left_subtree_sum -= z->exe_time;
        }
        current = p;
        p = current->parent;
    }

    if (z->left == NULL) {
        x = z->right;
        transplant(binary_tree, z, z->right);

    } else if (z->right == NULL) {
        x = z->left;
        transplant(binary_tree, z, z->left);

    }

    deleteNode(binary_tree, z);
    if (y_original_color == BLACK) {
        deleteFixup(binary_tree, x);
    }
}

// Function to delete a value from a binary search tree
void delete_i(struct binary_tree *binary_tree, uint64_t deadline, uint64_t exe_time, bool *deleted) {

    assert(binary_tree != NULL);

    struct TreeNode *z = searchByKey(binary_tree, deadline);
    if (z != NULL) {
        //if there are duplicated nodes in Z,
        //we just need to remove duplicated one.
        if (z->dup_next != NULL) {
            struct TreeNode* cur = z;
            struct TreeNode* prev = NULL;
            while (cur && cur->exe_time != exe_time) {
                prev = cur;
                cur = cur->dup_next;
            }

            //if the target node has been found
            if (cur != NULL) {
                //update the sumLeft in all of its parent node.
                struct TreeNode* current = z;
                struct TreeNode* p = current->parent;
                while (p != NULL) {
                    if (p->left == current)
                    {
                        p->left_subtree_sum -= exe_time;
                    }
                    current = p;
                    p = current->parent;
                }
            } else {
		*deleted = false;
		printf("not found the node (%d %d)\n", deadline, exe_time);
		return;
	    }
	    
            //if the removed node is the head of the linkedlist;
            if (cur == z) {
                //copy the data from the removed node.
                struct TreeNode* newroot = z->dup_next;
                newroot->color = z->color;
                newroot->left_subtree_sum = z->left_subtree_sum;
                newroot->left = z->left;
                if (newroot->left != NULL) {
                    newroot->left->parent = newroot;
                }
		newroot->right = z->right;
                if (newroot->right != NULL) {
                    newroot->right->parent = newroot;
                }

                newroot->parent = z->parent;

                if (z->parent) {
                    if (z->parent->left == z) {
                        z->parent->left = newroot;
                    } else {
                        z->parent->right = newroot;
                    }
                }
                //clean up the remove node z;
                //memset( z, 0, sizeof(Node));
		binary_tree->queue_length--;
		*deleted = true;
		deleteNode(binary_tree, z);
            }
            else { //remove the node from the link list;
                prev->dup_next = cur->dup_next;
            }
        } else{
	    if (z->exe_time == exe_time) {
                removeNode(binary_tree, z);
	        binary_tree->queue_length--;
	        *deleted = true;
	    } else {
		*deleted = false;
		printf("not found the node (%d %d)\n", deadline, exe_time);
	    }
        }
    } else {
	*deleted = false;
	printf("not found the node (%d %d)\n", deadline, exe_time);
    }
}

// Function to find a value in a binary search tree (non-recursive)
/*struct TreeNode* find(struct TreeNode* root, int val) {
    while (root != NULL) {
        if (val == root->val) {
            return root; // Return the node if value is found
        } else if (val < root->val) {
            root = root->left; // Move to left subtree if value is less
        } else {
            root = root->right; // Move to right subtree if value is greater
        }
    }
    return NULL; // Return NULL if value is not found or if the tree is empty
}*/

bool is_empty(struct binary_tree *binary_tree) {
	assert(binary_tree != NULL);

	return binary_tree->root == NULL;
}

void inorder(struct binary_tree *binary_tree, struct TreeNode* root)
{
    assert(binary_tree != NULL);

    if(root == NULL)
        return;
    inorder(binary_tree, root->left);
    printf("%lu(%lu) ", root->deadline, root->exe_time);
    inorder(binary_tree, root->right);
}
// return the sum of nodes' execution time that less than the target priority
uint64_t findMaxValueLessThan(struct binary_tree *binary_tree, struct TreeNode* root, uint64_t deadline) {
    assert(binary_tree != NULL);
    
    if (root == NULL) {
        return 0;
    }
    if (deadline == root->deadline) {
        return root->left_subtree_sum;
    } else if (deadline < root->deadline) {
        return findMaxValueLessThan(binary_tree, root->left, deadline);
    } else {
        return get_sum_exe_time_of_node(root) + root->left_subtree_sum + findMaxValueLessThan(binary_tree, root->right, deadline);
    }

}

struct TreeNode* makeEmpty(struct binary_tree *binary_tree, struct TreeNode* root)
{
    assert(binary_tree != NULL);
    
    if(root != NULL) {
        makeEmpty(binary_tree, root->left);
        makeEmpty(binary_tree, root->right);
        deleteNode(binary_tree, root);
    }
    return NULL;
}

int main() {
	struct binary_tree * bst = init_binary_tree(12, MAX_NODES); 
	for(int i = 0; i < 64; i++) {
		insert(bst, i+1, i+1);
	}
	print_tree_in_order(bst);
	printf("\n");
        
        printf("=============================================\n");
        for (int i = 0; i < 64; i++) {
            printf("Sum of values less than %d: %lu\n", i+1, findMaxValueLessThan(bst, bst->root, i+1));
        }

	printf("\n============insert the duplicated value: 8===============" );
	insert(bst, 8, 8);
       
        for (int i = 0; i < 64; i++) {
            printf("Sum of values less than %d: %lu\n", i+1, findMaxValueLessThan(bst, bst->root, i+1));
        }

	printf("\n============insert the duplicated value: 10===============" );
	insert(bst, 10, 10);
        for (int i = 0; i < 64; i++) {
            printf("Sum of values less than %d: %lu\n", i+1, findMaxValueLessThan(bst, bst->root, i+1));
        }
	
	printf("\n============remove the duplicated value: 8===============" );
	bool deleted = false;
	delete_i(bst, 8, 8, &deleted);
	for (int i = 0; i < 64; i++) {
            printf("Sum of values less than %d: %lu\n", i+1, findMaxValueLessThan(bst, bst->root, i+1));
        }
	
	printf("\n============remove the duplicated value: 10===============" );
	delete_i(bst, 10, 10, &deleted);
	for (int i = 0; i < 64; i++) {
            printf("Sum of values less than %d: %lu\n", i+1, findMaxValueLessThan(bst, bst->root, i+1));
        }
	
	printf("\n============remove the single value: 20===============" );
	delete_i(bst, 20, 20, &deleted);
        for (int i = 0; i < 64; i++) {
            printf("Sum of values less than %d: %lu\n", i+1, findMaxValueLessThan(bst, bst->root, i+1));
        }
	printf("\n============insert the duplicated value: 41===============" );
	insert(bst, 41, 35);
	print_tree_in_order(bst);
        printf("\n");

	for (int i = 0; i < 64; i++) {
            printf("Sum of values less than %d: %lu\n", i+1, findMaxValueLessThan(bst, bst->root, i+1));
        }

	printf("\n============remove the single value: 40===============" );	

	delete_i(bst, 40, 40, &deleted);
	print_tree_in_order(bst);
        printf("\n");
    	for (int i = 0; i < 64; i++) {
            printf("Sum of values less than %d: %lu\n", i+1, findMaxValueLessThan(bst, bst->root, i+1));
        }

	struct TreeNode *min = findMin(bst);
        printf("min is (%u %u)\n", min->deadline, min->exe_time);
	struct TreeNode *max = findMax(bst);
	printf("max is (%u %u)\n", max->deadline, max->exe_time);

	printf("delete 45, 30\n");
	delete_i(bst, 45, 30, &deleted);
	printf("delete 41, 35\n");
	delete_i(bst, 41, 35, &deleted);
	print_tree_in_order(bst);
        printf("\n");
}
