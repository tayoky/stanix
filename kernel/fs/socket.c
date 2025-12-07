#include <kernel/socket.h>
#include <kernel/list.h>

static list_t *socket_domains;

void init_sockets(void) {
	socket_domains = new_list();
}

static ssize_t socket_read(vfs_node *node, void *buf, uint64_t offset, size_t count) {
	socket_t *socket = (socket_t*)node;
}

static socket_t *add_defaults(socket_t *socket) {
	if (!socket) return NULL;
	socket->node.read = socket_read;
	return socket;
}

socket_t *create_socket(int domain, int type, int protocol) {
	foreach (node, socket_domains) {
		socket_domain_t *cur_domain = node->value;
		if (cur_domain->domain == domain) {
			return add_defaults(cur_domain->create(type, protocol));
		}
	}

	return NULL;
}

void register_socket_domain(socket_domain_t *domain) {
	list_append(socket_domains, domain);
}

void unregister_socket_domain(socket_domain_t *domain) {
	list_remove(socket_domains, domain);
}