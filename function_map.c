// NB: This is not very efficient. Ideally we should rebalance the whole thing either when necessary, or after loading each symtab.


#include "map.h"
#include "machine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

struct tree_node_t
{
   char* function;
   char* module;
   uint32_t address;
   struct tree_node_t* left;
   struct tree_node_t* right;  
};

typedef struct tree_node_t tree_node_t;

tree_node_t* function_map = NULL;

void found_function(char* module, char* function, uint32_t address)
{
   tree_node_t** f = &function_map;
   while (1)
   {     
      if (*f == NULL)
      {
         *f = malloc(sizeof(tree_node_t));
         (*f)->function = strdup(function);
         (*f)->module = strdup(module);
         (*f)->address = address;         
         (*f)->left = NULL;
         (*f)->right = NULL;
         break;
      }
      else
      {
         if (address < (*f)->address)
         {  // Look left. This is the easy case
            f = &(*f)->left;
         }
         else
         {
            if ((*f)->right != NULL && (*f)->right->address > address)
            {
               tree_node_t* new_node = malloc(sizeof(tree_node_t));
               new_node->function = strdup(function);
               new_node->module = strdup(module);
               new_node->address = address;         
               new_node->left = NULL;
               new_node->right = (*f)->right;
               (*f)->right = new_node;
               break;
               // Make (*f)->right the child of the new node
            }
            else
            {  // Look right
               f = &(*f)->right;
            }
         }
      }
   }
}

int lookup_function(uint32_t address, char** module, char** function)
{
   tree_node_t** f = &function_map;
   while (1)
   {
      if ((*f) == NULL)
         return 0;
      if ((*f)->address > address)
      {  // Go left 
         f = &(*f)->left;
      }
      else
      {
         if ((*f)->right != NULL && ((*f)->right->address > address))
         {
            // Found it!
            *module = (*f)->module;
            *function = (*f)->function;
            return 1;
         }
         f = &(*f)->right;
      }
   }
}
