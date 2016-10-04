## CAUTION

"void *spec" in plug_in structure is used for implementing inheritance.

"spec" will contain not common variable of the plug_in structure.
(e.g. MYSQL *mysql, FILE *fd)


DO NOT COLLECT METRIC IN THIS VARIABLE.

It is waste of memory if you do metric=>spec=>buf.

Put the metric values in the buf directly. (metric=>buf)