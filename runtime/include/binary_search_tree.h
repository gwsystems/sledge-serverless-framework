#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "memlogging.h"
#include "lock.h"

#define MAX_NODES 4096 // Maximum number of nodes in the pool

typedef uint64_t (*binary_tree_get_priority_fn_t)(void *data);
typedef uint64_t (*binary_tree_get_execution_cost_fn_t)(void *data, int thread_id);

// Definition of a binary search tree node
struct TreeNode {
    struct TreeNode *left;
    struct TreeNode *right;
    struct TreeNode *next;  // pointing to the next node, this is used for nodePool 
                            // to find next available node
    struct TreeNode *dup_next; // pointing to next duplicate key item
    uint64_t        left_subtree_sum;
    void            *data;  // sandbox 
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
        nodes[i].left_subtree_sum = 0;
        nodes[i].data  = NULL;
    }
    nodes[MAX_NODES - 1].next = NULL;
}

struct binary_tree * init_binary_tree(bool use_lock, binary_tree_get_priority_fn_t get_priority_fn, 
				      binary_tree_get_execution_cost_fn_t get_execution_cost_fn, int id, int queue_size) {
	
	assert(get_priority_fn != NULL);

	struct binary_tree *binary_tree = (struct binary_tree *)calloc(1, sizeof(struct binary_tree));
	initNodePool(&binary_tree->nodePool, queue_size);
	binary_tree->root = NULL;
	binary_tree->get_priority_fn = get_priority_fn;
	binary_tree->get_execution_cost_fn = get_execution_cost_fn;
	binary_tree->use_lock = use_lock;
	binary_tree->id = id;
        binary_tree->queue_length = 0;
	
	if (binary_tree->use_lock) lock_init(&binary_tree->lock);

	return binary_tree;
}

// Function to get the total number of non-deleted nodes in the binary tree
int getNonDeletedNodeCount(struct binary_tree *binary_tree) {
    assert(binary_tree != NULL);
    return binary_tree->queue_length;
}

// Get a new node from the pool
struct TreeNode* newNode(struct binary_tree *binary_tree, void *data) {
    
    assert(binary_tree != NULL);

    if (binary_tree->nodePool.head == NULL) {
        panic("Binary search tree queue %d is full\n", binary_tree->id);
        return NULL;
    } else {
        // Remove a node from the head of the memory pool
        struct TreeNode* new_node_t = binary_tree->nodePool.head;
        binary_tree->nodePool.head = new_node_t->next;
        new_node_t->next = NULL;  // Reset the next pointer of the new node
        new_node_t->data = data;
        new_node_t->left = NULL;
        new_node_t->right = NULL;
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
	if (node->data) {
            mem_log("%lu(%lu) ", ((struct sandbox *)node->data)->absolute_deadline, ((struct sandbox *)node->data)->estimated_cost);
	}
        //Print data in the dup_next list
        struct TreeNode* cur = node->dup_next;
        while(cur) {
           mem_log("%lu(%lu) ", ((struct sandbox *)cur->data)->absolute_deadline, ((struct sandbox *)cur->data)->estimated_cost);
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
        mem_log("\n");
    }
}

//get the total execute time of a node.
uint64_t get_sum_exe_time_of_node(struct binary_tree *binary_tree, struct TreeNode* node, int thread_id){
    uint64_t total = 0;
    struct TreeNode* curNode = node;
    while (curNode!=NULL) {
        total += binary_tree->get_execution_cost_fn(curNode->data, thread_id);
        curNode = curNode->dup_next;
    }

    return total;
}

// Return a node to the pool
void deleteNode(struct binary_tree *binary_tree, struct TreeNode* node) {

    assert(binary_tree != NULL);
    assert(node != NULL);

    // Insert the node back to the head of the memory pool
    node->left = NULL;
    node->right = NULL;
    node->data = NULL;
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

// Function to insert a value into a binary search tree
struct TreeNode* insert(struct binary_tree *binary_tree, struct TreeNode* root, void *data, int thread_id) {
    
    assert(binary_tree != NULL);

    if (root == NULL) {
        binary_tree->queue_length++;
        return newNode(binary_tree, data); // Create a new node for an empty tree
    }

    if (binary_tree->get_priority_fn(data) == binary_tree->get_priority_fn(root->data)) {
        //go to the tail of clone chain
        struct TreeNode* tail = root;
        while (tail->dup_next != NULL){
            tail = tail->dup_next;
        }

        //append the new node to the chain 
        struct TreeNode* new_node = newNode(binary_tree, data);
        tail->dup_next = new_node;
        binary_tree->queue_length++;
        return root;
    }

    if (binary_tree->get_priority_fn(data) < binary_tree->get_priority_fn(root->data)) {
        root->left = insert(binary_tree, root->left, data, thread_id); // Insert into the left subtree
        root->left_subtree_sum += binary_tree->get_execution_cost_fn(data, thread_id);
    } else {
        root->right = insert(binary_tree, root->right, data, thread_id); // Insert into the right subtree
    }
    return root;
}

// Helper function to find the minimum value in a binary search tree
struct TreeNode* findMin(struct binary_tree *binary_tree, struct TreeNode *root) {
    
    assert(binary_tree != NULL);

    if (root == NULL) {
        return NULL;
    }

    if (binary_tree->queue_length == 1) {
	return root;
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

struct TreeNode* remove_node_from_dup_list(struct binary_tree *binary_tree, struct TreeNode* root, void *data, bool *deleted) {
    if (root->data == data) {
        root->dup_next->left = root->left;
        root->dup_next->right = root->right;
        root->dup_next->left_subtree_sum = root->left_subtree_sum;
        //free old root
        struct TreeNode* new_root = root->dup_next;
        deleteNode(binary_tree, root);
        *deleted = true;
        binary_tree->queue_length--;
        return new_root;
    } else {
        struct TreeNode* cur = root->dup_next;
        struct TreeNode* pre = root;
        while(cur) {
            if (cur->data == data) {
                pre->dup_next = cur->dup_next;
                //free cur
                deleteNode(binary_tree, cur);
                *deleted = true;
                binary_tree->queue_length--;
                return root;
            } else {
                pre = cur;
                cur = cur->dup_next;
            }
        }
        *deleted = false;
        return root;
    }
}

// Function to delete a value from a binary search tree
struct TreeNode* delete_i(struct binary_tree *binary_tree, struct TreeNode* root, void *data, bool *deleted, int thread_id) {

    assert(binary_tree != NULL);

    if (root == NULL) {
	*deleted = false;
        return NULL;
    }

    int64_t cmp_result = binary_tree->get_priority_fn(data) - binary_tree->get_priority_fn(root->data);
    if (cmp_result < 0) {
        root->left_subtree_sum -= binary_tree->get_execution_cost_fn(data, thread_id); 
        root->left = delete_i(binary_tree, root->left, data, deleted, thread_id);
        return root;
    } else if (cmp_result > 0) {
        root->right = delete_i(binary_tree, root->right, data, deleted, thread_id);
        return root;
    } else { // cmp_result == 0
        // The deleted node might either be the root or in the dup_next list
        if (root->dup_next != NULL) {
             struct TreeNode* new_root = remove_node_from_dup_list(binary_tree, root, data, deleted);
             return new_root;
        }
        // If key is same as root's key, then this is the node to be deleted
        // Node with only one child or no child
        if (root->left == NULL) {
            struct TreeNode* temp = root->right;
            deleteNode(binary_tree, root);
            *deleted = true;
            binary_tree->queue_length--;
            return temp;
        } else if (root->right == NULL) {
            struct TreeNode* temp = root->left;
            deleteNode(binary_tree, root);
            *deleted = true;
            binary_tree->queue_length--;
            return temp;
        } else {
            // Node with two children: Get the inorder successor(smallest in the right subtree)
            struct TreeNode* succParent = root;
            struct TreeNode* succ = root->right;
            while (succ->left != NULL) {
                succParent = succ;
                succ = succ->left;
            }
            // Copy the inorder successor's content to this node, left_subtree_sum is not changed
            root->data = succ->data;
            root->dup_next = succ->dup_next;
            //update the sum_less_than of the nodes affected by the removed node.
            int removed_exe_time = get_sum_exe_time_of_node(binary_tree, succ, thread_id);
            struct TreeNode* temp = root->right;
            while (temp->left != NULL) {
                temp->left_subtree_sum -= removed_exe_time;
                temp = temp->left;
            }

            // Delete the inorder successor
            if (succParent->left == succ) {
                succParent->left = succ->right;
            } else {
                succParent->right = succ->right;
            }

            deleteNode(binary_tree, succ);
            *deleted = true;
            binary_tree->queue_length--;
            return root;
        }

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
    printf("%lu ", binary_tree->get_priority_fn(root->data));
    inorder(binary_tree, root->right);
}
// return the sum of nodes' execution time that less than the target priority
uint64_t findMaxValueLessThan(struct binary_tree *binary_tree, struct TreeNode* root, void *target, int thread_id) {
    assert(binary_tree != NULL);
    
    if (root == NULL) {
        return 0;
    }
    if (binary_tree->get_priority_fn(target) == binary_tree->get_priority_fn(root->data)) {
        return root->left_subtree_sum;
    } else if (binary_tree->get_priority_fn(target) < binary_tree->get_priority_fn(root->data)) {
        return findMaxValueLessThan(binary_tree, root->left, target, thread_id);
    } else {
        return get_sum_exe_time_of_node(binary_tree, root, thread_id) + root->left_subtree_sum + findMaxValueLessThan(binary_tree, root->right, target, thread_id);
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

