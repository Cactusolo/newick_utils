#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "enode.h"
#include "rnode.h"
#include "redge.h"
#include "link.h"
#include "list.h"
#include "tree.h"
#include "parser.h"
#include "to_newick.h"
#include "address_parser.h"
#include "tree_editor_rnode_data.h"
#include "common.h"

void address_scanner_set_input(char *);
void address_scanner_clear_input();
int adsparse();

enum action { ACTION_DELETE, ACTION_SUBTREE, ACTION_SPLICE_OUT,
	ACTION_PRINT_LABEL };

enum order { POST_ORDER, PRE_ORDER };

struct enode *expression_root;

struct parameters {
	char * address;
	int action;
	int show_tree;
	int order;
	int stop_clade_at_first_match;
};

void help(char *argv[])
{
	printf (
"Performs actions on nodes that match some condition\n"
"\n"
"Synopsis\n"
"--------\n"
"\n"
"%s [-nh] <newick trees filename|-> <address> <action>\n"
"\n"
"Input\n"
"-----\n"
"\n"
"First argument is the name of a file that contains Newick trees, or '-' (in\n"
"which case trees are read from standard input).\n"
"\n"
"Second argument is a node address, in the form of a logical expression (see\n"
"Addresses below).\n"
"\n"
"Third argument is a code that specifies an action to perform on nodes\n"
"which match the address (see Actions).\n" 
"\n"
"Output\n"
"------\n"
"\n"
"By default, prints the input tree, which may have been modified. However,\n"
"the 's' action (see Actions, below) causes matching subtrees to be\n"
"printed out.\n"
"\n"
"This program is analogous to pattern-oriented, stream processing UNIX\n"
"utilities like sed(1) and awk(1), but instead of working on lines (like\n"
"sed) or records (like awk), %s works on tree nodes.\n"
"\n"
"The program traverses the tree in Newick order, evaluating the address\n"
"expression for each node in turn. If (and only if) the address matches, the\n"
"action is performed.\n"
"\n"
"Addresses\n"
"---------\n"
"\n"
"The address expression involves node properties such as depth, bootstrap\n"
"support, whether or not a node is a leaf, etc. These are represented by\n"
"single-letter codes, to make expressions short. For example:\n"
"\n"
"				       i\n"
"\n"
"matches internal nodes, while\n"
"\n"
"				     b > 75\n"
"\n"
"matches nodes whose label has a numerical value of 75 or more (if the label\n"
"is numeric). The usual logical and relational operators are available, so\n"
"\n"
"				   i & b > 75\n"
"\n"
"could be used to match internal nodes with a bootstrap support value\n"
"greater than 75.\n"
"\n"
"The functions are:\n"
"    a  numeric    number of ancestors of node	\n"
"    b  numeric    node's support value (or zero)\n"
"    d  numeric    node's depth (distance to root)\n"
"    i	boolean    true iff node is strictly internal (i.e., not root!)\n"
"    l	boolean    true iff node is a leaf\n"
"    r	boolean    true iff node is the root\n"
"\n"
"The operators are:\n"
"    ==  equality\n"
"    !=  inequality\n"
"    <   greater than\n"
"    >   lesser than\n"
"    >=  greater than or equal to\n"
"    <=  lesser than or equal to\n"
"    !   logical negation\n"
"    &   logical and\n"
"    |   logical or\n"
"\n"
"The operator precedence is: relationals, and, or; i.e. \n"
"\n"
"				 1 == d & i | l\n"
"\n"
"is equivalent to\n"
"\n"
"			       ((1 == d) & i) | l\n"
"\n"
"Parentheses can be used for overriding precedence, or for clarity.\n"
"\n"
"Actions\n"
"-------\n"
"\n"
"Actions are performed on nodes that match the address. They are:\n"
"    s   (Subtree) print subtree rooted at matching node\n"
"    o   (splice Out) splice out node, and attach children to parent, \n"
"	   preserving branch lengths. This is useful for \"opening\" poorly\n"
"          supported nodes.\n"
"    d   Delete node\n"
"    l   Print node's label\n"   
"\n"
"\n"
"Options\n"
"-------\n"
"\n"
"    -h: print this help text, and exit\n"
"    -n: do not print the (possibly modified) tree at the end of the run \n"
"        (modeled after sed -n)\n"
"    -r: visit tree in preorder (starting at root, and visiting a node\n"
"        before any of its descendants). Default is post-order (ends at root\n"
"        and visits a node after all its descendats).\n"
"    -o: stop processing a clade after the first match - that is, if a node\n"
"        matches, its descendants are not processed.\n"
"        Note: this option will automatically set -r, as it makes no\n"
"        sense in post-order.\n"
"\n"
"Bugs\n"
"----\n"
"\n"
"Although there are no known bugs in this program, it is to be considered\n"
"more experimental than the others.\n"
"\n"
"Examples\n"
"--------\n"
"\n"
"# \"open\" all nodes with bootstrap support <= 10 (assuming support is coded\n"
"# in internal node labels)\n"
"\n"
"%s data/HRV.bs.nw 'i & b <= 10' o \n"
"\n"
"# \"open\" all nodes with bootstrap support < 750, then discard leaves that\n"
"# are directly attached to the ingroup's root. This effectively keeps only\n"
"# leaves that are part of well-supported clades.\n"
"\n"
"%s data/big.rn.nw 'i & b < 750' o | %s - 'l & a == 2' d\n"
"\n"
"# get all clades with at least one ancestor, 980 or better support. Do not\n"
"# print subtrees of matching clades, even if they match (option -o)\n"
"\n"
"%s data/big.rn.nw -n -o 'a >= 1 & b >= 980' s\n",
	argv[0],
	argv[0],
	argv[0],
	argv[0],
	argv[0],
	argv[0]
	);
}

struct parameters get_params(int argc, char *argv[])
{
	struct parameters params;

	params.show_tree = TRUE;
	params.order = POST_ORDER;
	params.stop_clade_at_first_match = FALSE;

	int opt_char;
	while ((opt_char = getopt(argc, argv, "hnor")) != -1) {
		switch (opt_char) {
		case 'h':
			help(argv);
			exit(EXIT_SUCCESS);
		case 'n':
			params.show_tree = FALSE;
			break;
		case 'o':
			params.stop_clade_at_first_match = TRUE;
			params.order = PRE_ORDER;
			break;
		case 'r':
			params.order = PRE_ORDER;
			break;
		default:
			fprintf (stderr, "Unknown option '-%c'\n", opt_char);
			exit (EXIT_FAILURE);
		}
	}

	/* check arguments */
	if (3 == (argc - optind))	{
		if (0 != strcmp("-", argv[optind])) {
			FILE *fin = fopen(argv[optind], "r");
			extern FILE *nwsin;
			if (NULL == fin) {
				perror(NULL);
				exit(EXIT_FAILURE);
			}
			nwsin = fin;
		}
		params.address = argv[optind+1];
		char action = argv[optind+2][0];
		switch (action) {
		case 's': params.action = ACTION_SUBTREE;
			break;
		case 'o': params.action = ACTION_SPLICE_OUT;
			break;
		case 'd': params.action = ACTION_DELETE;
		  	break;
		case 'l': params.action = ACTION_PRINT_LABEL;
		 	break;
		default: fprintf(stderr, 
			"Action '%c' is unknown.\n", action);
			 exit(EXIT_FAILURE);
		}
	} else {
		fprintf(stderr, "Usage: %s [-hn] <filename|-> <address> <operation>\n",
				argv[0]);
		exit(EXIT_FAILURE);
	}

	return params;
}

/* This allocates the rnode_data structure for each node, and fills it with
 * "top-down" data,  i.e. data for which the parent's value needs to be known
 * (such as depth and number of ancestors). */

void reverse_parse_order_traversal(struct rooted_tree *tree)
{
	struct list_elem *el;
	struct llist *rev_nodes = llist_reverse(tree->nodes_in_order);
	struct rnode *node;
	struct rnode_data *rndata;

	el = rev_nodes->head;	/* root */
	node = (struct rnode *) el->data;
	rndata = malloc(sizeof(struct rnode_data));
	if (NULL == rndata) { perror(NULL); exit (EXIT_FAILURE); }
	rndata->nb_ancestors = 0;
	rndata->depth = 0;
	node->data = rndata;

	for (el = rev_nodes->head->next; NULL != el; el = el -> next) {
		node = (struct rnode *) el->data;
		struct rnode *parent = node->parent_edge->parent_node;
		struct rnode_data *parent_data = (struct rnode_data *) parent->data;
		rndata = malloc(sizeof(struct rnode_data));
		if (NULL == rndata) { perror(NULL); exit (EXIT_FAILURE); }
		rndata->nb_ancestors = parent_data->nb_ancestors + 1;
		rndata->depth = parent_data->depth +
			atof(node->parent_edge->length_as_string);

		node->data = rndata;
	}

	destroy_llist(rev_nodes);
}

/* This fills bottom-up data. Note that it relies on rnode_data being already
 * allocated, which is done in reverse_parse_order_traversal(). Data that does
 * not depend on order is also filled in here. */

void parse_order_traversal(struct rooted_tree *tree)
{
	struct list_elem *el;
	struct rnode *node;
	struct rnode_data *rndata;

	/* NOTE: for now there is no bottom-up data, but this is where it will
	 * be set (e.g., number of descendants, etc) */
	for (el = tree->nodes_in_order->head; NULL != el; el = el -> next) {
		node = (struct rnode *) el->data;
		rndata = (struct rnode_data *) node->data;
		rndata->support = atof(node->label);	
	}
}

void process_tree(struct rooted_tree *tree, struct parameters params)
{
	struct llist *nodes;
	struct list_elem *el;
	struct rnode *r;
	char *newick;

	/* these two traversals fill the node data. */
	reverse_parse_order_traversal(tree);
	parse_order_traversal(tree);

	if (POST_ORDER == params.order)
		nodes = tree->nodes_in_order;
	else if (PRE_ORDER == params.order)
		nodes = llist_reverse(tree->nodes_in_order);
	else 
		assert(0);	 /* programmer error... */


	/* Main loop: Iterate over all nodes */
	for (el = nodes->head; NULL != el; el = el -> next) {
		struct rnode *current = (struct rnode *) el->data;

		/* Check for stop mark in parent (see option -o) */
		if (! is_root(current)) { 	/* root has no parent... */
			if (((struct rnode_data *)
			current->parent_edge->parent_node->data)->stop_mark) {
				/* Stop-mark the current node and continue */ 
				((struct rnode_data *)
				current->data)->stop_mark = TRUE;
				continue;
			}
		} 

		/* Set the current node for evaluation, and evaluate the
		 * expression - if the current node matches, then perform the
		 * specified action */
		enode_eval_set_current_rnode(current);
		if (eval_enode(expression_root)) {
			switch (params.action) {
			case ACTION_SUBTREE:
				newick = to_newick(current);
				printf("%s\n", newick);
				free(newick);
				break;
			case ACTION_SPLICE_OUT:
				if (is_inner_node(current)) {
					splice_out_rnode(current);
				} else {
					fprintf (stderr, "Warning: tried to splice out non-inner node ('%s')\n", current->label);
				}
				break;
			case ACTION_DELETE:
				r = unlink_rnode(current);
				if (NULL != r) {
					r->parent_edge->parent_node = NULL;
					// TODO: this causes a bug (try
					// nw_ed data/completely_labeled.nw i d
					tree->root = r;
				} 
				break;
			case ACTION_PRINT_LABEL:
				printf ("%s\n", current->label);
				break;
			default: fprintf (stderr,
				"Unknown action %d.\n", params.action);
				 exit(EXIT_FAILURE);
			}

			/* If -o switch is set, we stop processing this clade
			 * since this node matches -  we set the stop mark on
			 * this node */
			if (params.stop_clade_at_first_match)
				((struct rnode_data *) current->data)->stop_mark = TRUE;
		} /* matching node */	
	}

	/* If order is PRE_ORDER, the list of nodes is an inverted copy, so we
	 * need to free it. */
	if (PRE_ORDER == params.order)
		destroy_llist(nodes);
}

int main(int argc, char* argv[])
{
	struct parameters params = get_params(argc, argv);
	struct rooted_tree *tree;

	address_scanner_set_input(params.address);
	adsparse(); /* sets 'expression_root' TODO: make change of global explicit */
	address_scanner_clear_input();

	while (NULL != (tree = parse_tree())) {
		process_tree(tree, params);
		if (params.show_tree) {
			char *newick = to_newick(tree->root);
			printf("%s\n", newick);
			free(newick);
		}
		destroy_tree(tree, FREE_NODE_DATA);
	}

	return 0;
}
