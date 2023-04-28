#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "lock.h"

#define MAX_NODES 1024 // Maximum number of nodes in the pool

typedef uint64_t (*binary_tree_get_priority_fn_t)(void *data);
typedef uint64_t (*binary_tree_get_execution_cost_fn_t)(void *data, int thread_id);

// Definition of a binary search tree node
struct TreeNode {
    struct TreeNode *left;
    struct TreeNode *right;
    struct TreeNode *next;  // pointing to the next node
    void            *data; 
};

// Definition of TreeNode memory pool
struct TreeNodePool {
    struct TreeNode* head;
};

struct binary_tree {
	struct TreeNode 	            *root;
	struct TreeNodePool                 nodePool;
	binary_tree_get_priority_fn_t       get_priority_fn;
	binary_tree_get_execution_cost_fn_t get_execution_cost_fn;
	lock_t                              lock;
	bool                                use_lock;
};

// Initialize the node pool
void initNodePool(struct TreeNodePool *nodePool) {
    
    assert(nodePool != NULL);

    struct TreeNode *nodes = (struct TreeNode*)malloc(MAX_NODES * sizeof(struct TreeNode));
    nodePool->head = nodes;  // Initialize head to the beginning of the node array
 
    for (int i = 0; i < MAX_NODES - 1; ++i) {
        nodes[i].next  = &nodes[i + 1];  // Set the next pointer of each node to the next node
        nodes[i].left  = NULL;
	nodes[i].right = NULL;
	nodes[i].data  = NULL;
    }
    nodes[MAX_NODES - 1].next = NULL;
}

struct binary_tree * init_binary_tree(bool use_lock, binary_tree_get_priority_fn_t get_priority_fn, 
				      binary_tree_get_execution_cost_fn_t get_execution_cost_fn) {
	
	assert(get_priority_fn != NULL);

	struct binary_tree *binary_tree = (struct binary_tree *)calloc(1, sizeof(struct binary_tree));
	initNodePool(&binary_tree->nodePool);
	binary_tree->root = NULL;
	binary_tree->get_priority_fn = get_priority_fn;
	binary_tree->get_execution_cost_fn = get_execution_cost_fn;
	binary_tree->use_lock = use_lock;
	
	if (binary_tree->use_lock) lock_init(&binary_tree->lock);

	return binary_tree;
}

// Get a new node from the pool
struct TreeNode* newNode(struct binary_tree *binary_tree, void *data) {
    
    assert(binary_tree != NULL);

    if (binary_tree->nodePool.head == NULL) {
        panic("queue is full\n");
        return NULL;
    } else {
        // Remove a node from the head of the memory pool
        struct TreeNode* new_node_t = binary_tree->nodePool.head;
        binary_tree->nodePool.head = new_node_t->next;
        new_node_t->next = NULL;  // Reset the next pointer of the new node
        new_node_t->data = data;
        new_node_t->left = NULL;
        new_node_t->right = NULL;
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
// Return a node to the pool
void deleteNode(struct binary_tree *binary_tree, struct TreeNode* node) {

    assert(binary_tree != NULL);
    assert(node != NULL);

    // Insert the node back to the head of the memory pool
    node->left = NULL;
    node->right = NULL;
    node->next = binary_tree->nodePool.head;
    node->data = NULL;
    binary_tree->nodePool.head = node;
}

int findHeight(struct TreeNode *root)
{
    int lefth, righth;
    if(root == NULL)
        return -1;
    lefth = findHeight(root->left);
    righth = findHeight(root->right);
    return (lefth > righth ? lefth : righth)+1;
}

// Function to insert a value into a binary search tree
struct TreeNode* insert(struct binary_tree *binary_tree, struct TreeNode* root, void *data) {
    
    assert(binary_tree != NULL);

    if (root == NULL) {
        return newNode(binary_tree, data); // Create a new node for an empty tree
    }

    if (binary_tree->get_priority_fn(data) <= binary_tree->get_priority_fn(root->data)) {
        root->left = insert(binary_tree, root->left, data); // Insert into the left subtree
    } else {
        root->right = insert(binary_tree, root->right, data); // Insert into the right subtree
    }
    return root;
}

// Helper function to find the minimum value in a binary search tree
struct TreeNode* findMin(struct binary_tree *binary_tree, struct TreeNode *root) {
    
    assert(binary_tree != NULL);

    if (root == NULL) {
        return NULL;
    }

    lock_node_t node = {};
    lock_lock(&binary_tree->lock, &node);
    while (root->left != NULL) {
        root = root->left; // Keep traversing to the left until the leftmost node is reached
    }
    lock_unlock(&binary_tree->lock, &node);
    return root;
}

// Helper function to find the maximum value in a binary search tree
struct TreeNode* findMax(struct binary_tree *binary_tree, struct TreeNode *root) {
    if (root == NULL) {
        return NULL;
    }

    lock_node_t node = {};
    lock_lock(&binary_tree->lock, &node);
    while (root->right != NULL) {
        root = root->right; // Keep traversing to the right until the rightmost node is reached
    }
    lock_unlock(&binary_tree->lock, &node); 
    return root;
}

// Function to delete a value from a binary search tree
struct TreeNode* delete(struct binary_tree *binary_tree, struct TreeNode* root, void *data, bool *deleted) {

    assert(binary_tree != NULL);

    if (root == NULL) {
	*deleted = false;
        return NULL;
    }

    int64_t cmp_result = binary_tree->get_priority_fn(data) - binary_tree->get_priority_fn(root->data);
    if (cmp_result < 0) {
        root->left = delete(binary_tree, root->left, data, deleted);
    } else if (cmp_result > 0) {
        root->right = delete(binary_tree, root->right, data, deleted);
    } else { // cmp_result == 0
        if (root->data == data) {
                if (root->left == NULL) {
                        struct TreeNode* temp = root->right;
                        deleteNode(binary_tree, root);
			*deleted = true;
                        return temp;
                } else if (root->right == NULL) {
                        struct TreeNode* temp = root->left;
                        deleteNode(binary_tree, root);
			*deleted = true;
                        return temp;
                } else {
                        struct TreeNode* successor = root->right;
                        while (successor->left != NULL) {
                                successor = successor->left;
                        }
                        root->data = successor->data;
                        root->right = delete(binary_tree, root->right, successor->data, deleted);
                        return root;
                }
        } else {
                // Continue searching for the node with the same data pointer
                if (root->left != NULL) {
                        root->left = delete(binary_tree, root->left, data, deleted);
                }

		if (*deleted == false && root->right != NULL) {
                        root->right = delete(binary_tree, root->right, data, deleted);
                }
        }
   }
    return root;

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
    printf("%lu ", binary_tree->get_priority_fn(root->data));
    inorder(binary_tree, root->right);
}

struct TreeNode* findMaxValueLessThan(struct binary_tree *binary_tree, struct TreeNode* root, void *target, uint64_t *sum, int thread_id) {

    assert(binary_tree != NULL);

    if (root == NULL) {
	*sum = 0;
        return NULL; // Base case: empty node, return NULL
    }

    struct TreeNode* maxNode = NULL; // Initialize the node containing the maximum value to NULL

    // In-order traversal of the binary tree
    struct TreeNode* leftMaxNode = findMaxValueLessThan(binary_tree, root->left, target, sum, thread_id); // Traverse left subtree
    if (binary_tree->get_priority_fn(root->data) <= binary_tree->get_priority_fn(target)) {
        *sum += binary_tree->get_execution_cost_fn(root->data, thread_id); // Add the current node's value to the sum
        maxNode = root; // Update the maximum node
    }
    struct TreeNode* rightMaxNode = findMaxValueLessThan(binary_tree, root->right, target, sum, thread_id); // Traverse right subtree

    // Update the maximum node with the maximum node from left and right subtrees
    if (leftMaxNode != NULL && (maxNode == NULL || binary_tree->get_priority_fn(leftMaxNode->data) > binary_tree->get_priority_fn(maxNode->data))) {
        maxNode = leftMaxNode;
    }
    if (rightMaxNode != NULL && (maxNode == NULL || binary_tree->get_priority_fn(rightMaxNode->data) > binary_tree->get_priority_fn(maxNode->data))) {
        maxNode = rightMaxNode;
    }

    return maxNode;
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

