## CAUTION

"void *detail" in agent structure is for implementing inheritance.

"detail" will contain not common variable of agent.
(e.g. MYSQL *mysql, FILE *fd)

DO NOT COLLECT METRIC IN THIS VARIABLE.

It is waste of memery if you do metric=>detail=>buf.

Put the metric values in the buf directly. (metric=>buf)