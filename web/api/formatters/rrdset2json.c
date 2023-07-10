// SPDX-License-Identifier: GPL-3.0-or-later

#include "rrdset2json.h"

void chart_labels2json(RRDSET *st, BUFFER *wb, size_t indentation)
{
    if(unlikely(!st->rrdlabels))
        return;

    char tabs[11];

    if (indentation > 10)
        indentation = 10;

    tabs[0] = '\0';
    while (indentation) {
        strcat(tabs, "\t\t");
        indentation--;
    }

    rrdlabels_to_buffer(st->rrdlabels, wb, tabs, ":", "\"", ",\n", NULL, NULL, NULL, NULL);
    buffer_strcat(wb, "\n");
}

// generate JSON for the /api/v1/chart API call

void rrdset2json(RRDSET *st, BUFFER *wb, size_t *dimensions_count, size_t *memory_used, int skip_volatile) {
    time_t first_entry_t = rrdset_first_entry_s(st);
    time_t last_entry_t  = rrdset_last_entry_s(st);

    buffer_sprintf(
        wb,
        "\t\t{\n"
        "\t\t\t\"id\": \"%s\",\n"
        "\t\t\t\"name\": \"%s\",\n"
        "\t\t\t\"type\": \"%s\",\n"
        "\t\t\t\"family\": \"%s\",\n"
        "\t\t\t\"context\": \"%s\",\n"
        "\t\t\t\"title\": \"%s (%s)\",\n"
        "\t\t\t\"priority\": %d,\n"
        "\t\t\t\"plugin\": \"%s\",\n"
        "\t\t\t\"module\": \"%s\",\n"
        "\t\t\t\"units\": \"%s\",\n"
        "\t\t\t\"data_url\": \"/api/v1/data?chart=%s\",\n"
        "\t\t\t\"chart_type\": \"%s\",\n",
        rrdset_id(st),
        rrdset_name(st),
        rrdset_parts_type(st),
        rrdset_family(st),
        rrdset_context(st),
        rrdset_title(st),
        rrdset_name(st),
        st->priority,
        rrdset_plugin_name(st),
        rrdset_module_name(st),
        rrdset_units(st),
        rrdset_name(st),
        rrdset_type_name(st->chart_type));

    if (likely(!skip_volatile))
        buffer_sprintf(
            wb,
            "\t\t\t\"duration\": %"PRId64",\n",
            (int64_t)(last_entry_t - first_entry_t + st->update_every) //st->entries * st->update_every
        );

    buffer_sprintf(
        wb,
        "\t\t\t\"first_entry\": %"PRId64",\n",
        (int64_t)first_entry_t //rrdset_first_entry_t(st)
    );

    if (likely(!skip_volatile))
        buffer_sprintf(
            wb,
            "\t\t\t\"last_entry\": %"PRId64",\n",
            (int64_t)last_entry_t //rrdset_last_entry_t(st)
        );

    buffer_sprintf(
        wb,
        "\t\t\t\"update_every\": %d,\n"
        "\t\t\t\"dimensions\": {\n",
        st->update_every);

    unsigned long memory = sizeof(RRDSET);

    size_t dimensions = 0;
    RRDDIM *rd;
    rrddim_foreach_read(rd, st) {
        if(rrddim_option_check(rd, RRDDIM_OPTION_HIDDEN) || rrddim_flag_check(rd, RRDDIM_FLAG_OBSOLETE)) continue;

        memory += rrddim_size() + rd->db.memsize;

        if (dimensions)
            buffer_strcat(wb, ",\n\t\t\t\t\"");
        else
            buffer_strcat(wb, "\t\t\t\t\"");
        buffer_json_strcat(wb, rrddim_id(rd));
        buffer_strcat(wb, "\": { \"name\": \"");
        buffer_json_strcat(wb, rrddim_name(rd));
        buffer_strcat(wb, "\" }");

        dimensions++;
    }
    rrddim_foreach_done(rd);

    if(dimensions_count) *dimensions_count += dimensions;
    if(memory_used) *memory_used += memory;

    buffer_sprintf(wb, "\n\t\t\t},\n\t\t\t\"chart_variables\": ");
    health_api_v1_chart_custom_variables2json(st, wb);

    buffer_strcat(wb, ",\n\t\t\t\"green\": ");
    buffer_print_netdata_double(wb, st->green);
    buffer_strcat(wb, ",\n\t\t\t\"red\": ");
    buffer_print_netdata_double(wb, st->red);

    if (likely(!skip_volatile)) {
        buffer_strcat(wb, ",\n\t\t\t\"alarms\": {\n");
        size_t alarms = 0;
        RRDCALC *rc;
        rw_spinlock_read_lock(&st->alerts.spinlock);
        DOUBLE_LINKED_LIST_FOREACH_FORWARD(st->alerts.base, rc, prev, next) {
            buffer_sprintf(
                wb,
                "%s"
                "\t\t\t\t\"%s\": {\n"
                "\t\t\t\t\t\"id\": %u,\n"
                "\t\t\t\t\t\"status\": \"%s\",\n"
                "\t\t\t\t\t\"units\": \"%s\",\n"
                "\t\t\t\t\t\"update_every\": %d\n"
                "\t\t\t\t}",
                (alarms) ? ",\n" : "", rrdcalc_name(rc), rc->id, rrdcalc_status2string(rc->status), rrdcalc_units(rc),
                rc->update_every);

            alarms++;
        }
        rw_spinlock_read_unlock(&st->alerts.spinlock);
        buffer_sprintf(wb,
                       "\n\t\t\t}"
        );
    }
    buffer_strcat(wb, ",\n\t\t\t\"chart_labels\": {\n");
    chart_labels2json(st, wb, 2);
    buffer_strcat(wb, "\t\t\t}");

    buffer_strcat(wb, ",\n\t\t\t\"functions\": {\n");
    chart_functions2json(st, wb, 4, "\"", "\"");
    buffer_strcat(wb, "\t\t\t}");

    buffer_sprintf(wb,
            "\n\t\t}"
    );
}
