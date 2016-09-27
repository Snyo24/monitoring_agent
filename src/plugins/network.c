/** @file network_plugin.c @author Snyo */

#include "plugins/network.h"

#include "pluggable.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <zlog.h>

// TODO configuration parsing
plugin_t *new_network_plugin(const char *name, const char *conf) {
	plugin_t *plugin = new_plugin(1);
	if(!plugin) return NULL;

	// network detail setup
	network_detail_t *detail = (network_detail_t *)malloc(sizeof(network_detail_t));
	if(!detail) {
		zlog_error(plugin->tag, "Failed to allocate network detail");
		delete_plugin(plugin);
		return NULL;
	}

	plugin->type = "linux_linux_1.0";
	plugin->num   = 1;
	plugin->plugin_ip  = "test";
	plugin->target_ip = "test";

	// inheritance
	plugin->detail = detail;

	// polymorphism
	plugin->metric_names    = network_metric_names;
	plugin->collect_metrics = collect_network_metrics;
	plugin->delete = delete_plugin;

	return plugin;
}

void collect_network_metrics(plugin_t *plugin) {
	zlog_debug(plugin->tag, "Collecting metrics");

	json_object *values = json_object_new_array();
	/* Network */
	FILE *net_fp = popen("cat /proc/net/dev | grep : | awk \'{sub(\":\", \"\", $1); print $1, $2, $3, $10, $11}\'", "r");
	char net_name[100];
	int byte_in, byte_out;
	int pckt_in, pckt_out;
	if(fscanf(net_fp, "%s%d%d%d%d", net_name, &byte_in, &byte_out, &pckt_in, &pckt_out) == 5) {
		// json_object *net_info = json_object_new_array();
		// // bytes in and out
		// json_object_array_add(net_info, json_object_new_int(byte_in));
		// json_object_array_add(net_info, json_object_new_int(byte_out));
		// // packets in and out
		// json_object_array_add(net_info, json_object_new_int(pckt_in));
		// json_object_array_add(net_info, json_object_new_int(pckt_out));
		// json_object_object_add(net_json, net_name, net_info);

		// json_object_array_add(net_info, json_object_new_int(byte_in));
		// json_object_array_add(net_info, json_object_new_int(byte_out));
		// json_object_array_add(values, net_info);
		json_object_array_add(values, json_object_new_string(net_name));
		// bytes in and out
		json_object_array_add(values, json_object_new_int(byte_in));
		json_object_array_add(values, json_object_new_int(byte_out));
		// packets in and out
		json_object_array_add(values, json_object_new_int(pckt_in));
		json_object_array_add(values, json_object_new_int(pckt_out));
	}
	// json_object_array_add(values, net_json);
	pclose(net_fp);

	add_metrics(plugin, values);
}

void delete_network_plugin(plugin_t *plugin) {
	free(plugin->detail);
	delete_plugin(plugin);
}