/**
 * @file os.c
 * @author Snyo
 */
#include "plugins/os.h"

#include "pluggable.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <zlog.h>

// TODO configuration parsing
os_plugin_t *new_os_plugin() {
	os_plugin_t *plugin = new_plugin(1);
	if(!plugin) return NULL;

	plugin->tag = zlog_get_category("Agent_os");
	if(!plugin->tag) return NULL;

	plugin->num   = 1;
	plugin->type = "linux_linux_1.0";
	plugin->agent_ip = "test";
	plugin->target_ip = "test";

	// inheritance
	plugin->spec = NULL;

	// polymorphism
	plugin->job = collect_os_metrics;
	plugin->delete = delete_os_plugin;

	return plugin;
}

void collect_os_metrics(os_plugin_t *plugin) {
	zlog_debug(plugin->tag, "Collecting metrics");

	/* Network */
	FILE *net_fp = popen("cat /proc/net/dev | grep : | awk \'{sub(\":\", \"\", $1); print $1, $2, $3, $10, $11}\'", "r");
	char net_name[100];
	int byte_in, byte_out;
	int pckt_in, pckt_out;

	json_object *values = json_object_new_array();
	if(fscanf(net_fp, "%s%d%d%d%d", net_name, &byte_in, &byte_out, &pckt_in, &pckt_out) == 5) {
		json_object_array_add(values, json_object_new_string(net_name));
		// bytes in and out
		json_object_array_add(values, json_object_new_int(byte_in));
		json_object_array_add(values, json_object_new_int(byte_out));
		// packets in and out
		json_object_array_add(values, json_object_new_int(pckt_in));
		json_object_array_add(values, json_object_new_int(pckt_out));
	}
	pclose(net_fp);

	char ts[100];
	sprintf(ts, "%lu", plugin->next_run - plugin->period);
	json_object_object_add(plugin->values, ts, values);
	++plugin->stored;

	if(plugin->stored == 3) {
		pack(plugin);
		plugin->values = json_object_new_object();
		plugin->stored = 0;
	}
}

void delete_os_plugin(os_plugin_t *plugin) {
	if(!plugin) return;
	delete_plugin(plugin);
}