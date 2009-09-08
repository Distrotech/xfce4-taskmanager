/* $Id$
 *
 * Copyright (c) 2006  Johannes Zellner <webmaster@nebulon.de>
 *               2008  Mike Massonnet <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "functions.h"

static system_status *sys_stat =NULL;

gboolean refresh_task_list(void)
{
	gint i, j;
	GArray *new_task_list;
	gchar *cpu_tooltip, *mem_tooltip;
	gdouble cpu_usage;
	guint num_cpus;
	guint memory_used;

	if (sys_stat!=NULL)
	    num_cpus = sys_stat->cpu_count;
	else
	    num_cpus = 1;

	/* gets the new task list */
	new_task_list = (GArray*) get_task_list();

	/* check if task is new and marks the task that its checked*/
	for(i = 0; i < task_array->len; i++)
	{
		struct task *tmp = &g_array_index(task_array, struct task, i);
		tmp->checked = FALSE;

		for(j = 0; j < new_task_list->len; j++)
		{
			struct task *new_tmp = &g_array_index(new_task_list, struct task, j);

			if(new_tmp->pid == tmp->pid)
			{
/* FIXME */
#ifdef __linux
				tmp->old_time = tmp->time;
				tmp->time = new_tmp->time;

				tmp->old_time_percentage = tmp->time_percentage;
				tmp->time_percentage = (gdouble)(tmp->time - tmp->old_time) * (1.0/num_cpus);
#endif
#ifdef __sun
				tmp->old_time = tmp->time;
				tmp->time = new_tmp->time;

				tmp->old_time_percentage = tmp->time_percentage;
				tmp->time_percentage = (gdouble)(tmp->time - tmp->old_time) / (100.0*num_cpus);
#endif
				if(
				    tmp->ppid != new_tmp->ppid ||
				    strcmp(tmp->state,new_tmp->state) ||
				    tmp->vsize != new_tmp->vsize ||
				    tmp->rss != new_tmp->rss ||
#ifdef __linux || __sun
				    tmp->time_percentage != tmp->old_time_percentage ||
#endif
				    tmp->prio != new_tmp->prio
				 )
				{
					tmp->ppid = new_tmp->ppid;
					strcpy(tmp->state, new_tmp->state);
					tmp->vsize = new_tmp->vsize;
					tmp->rss = new_tmp->rss;
					tmp->prio = new_tmp->prio;

					refresh_list_item(i);
				}
				tmp->checked = TRUE;
				new_tmp->checked = TRUE;
				break;
			}
			else
				tmp->checked = FALSE;
		}
	}

	/* check for unchecked old-tasks for deleting */
	i = 0;
	while( i < task_array->len)
	{

		struct task *tmp = &g_array_index(task_array, struct task, i);

		if(!tmp->checked)
		{
			remove_list_item((gint)tmp->pid);
			g_array_remove_index(task_array, i);
			tasks--;
		}
		else
			i++;

	}


	/* check for unchecked new tasks for inserting */
	for(i = 0; i < new_task_list->len; i++)
	{
		struct task *new_tmp = &g_array_index(new_task_list, struct task, i);

		if(!new_tmp->checked)
		{
			struct task *new_task = new_tmp;

			g_array_append_val(task_array, *new_task);
			if((show_user_tasks && new_task->uid == own_uid) || (show_root_tasks && new_task->uid == 0) ||  (show_other_tasks && new_task->uid != own_uid && new_task->uid != 0))
				add_new_list_item(tasks);
			tasks++;
		}
	}

	g_array_free(new_task_list, TRUE);

	/* update the CPU and memory progress bars */
	if (sys_stat == NULL)
		sys_stat = g_new (system_status, 1);
	get_system_status (sys_stat);

	memory_used = sys_stat->mem_total - sys_stat->mem_free;
	if ( show_cached_as_free )
	{
		memory_used-=sys_stat->mem_cached;
		memory_used-=sys_stat->mem_buffers;
	}

	mem_tooltip = g_strdup_printf (_("%d MB of %d MB used"), memory_used/1024, sys_stat->mem_total/1024);
#if !GTK_CHECK_VERSION (2, 12, 0)
	gtk_tooltips_set_tip (tooltips, mem_usage_progress_bar_box, mem_tooltip, NULL);
#else
	gtk_widget_set_tooltip_text (mem_usage_progress_bar_box, mem_tooltip);
#endif
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (mem_usage_progress_bar),  (gdouble)memory_used/sys_stat->mem_total);

	cpu_usage = get_cpu_usage (sys_stat);
	cpu_tooltip = g_strdup_printf (_("%0.0f %%"), cpu_usage*100.0);
#if !GTK_CHECK_VERSION (2, 12, 0)
	gtk_tooltips_set_tip (tooltips, cpu_usage_progress_bar_box, cpu_tooltip, NULL);
#else
	gtk_widget_set_tooltip_text (cpu_usage_progress_bar_box, cpu_tooltip);
#endif
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (cpu_usage_progress_bar), cpu_usage);

	g_free (mem_tooltip);
	g_free (cpu_tooltip);

	return TRUE;
}

gdouble get_cpu_usage(system_status *sys_stat)
{
	gdouble cpu_usage = 0.0;
	guint current_jiffies;
	guint current_used;
	guint delta_jiffies;

	if ( get_cpu_usage_from_proc( sys_stat ) == FALSE )
	{
		gint i = 0;

		for(i = 0; i < task_array->len; i++)
		{
			struct task *tmp = &g_array_index(task_array, struct task, i);
			cpu_usage += tmp->time_percentage;
		}

		cpu_usage = cpu_usage / (sys_stat->cpu_count * 100.0);
	} else {

		if ( sys_stat->cpu_old_jiffies > 0 ) {
			current_used =
				sys_stat->cpu_user +
				sys_stat->cpu_nice +
				sys_stat->cpu_system;
			current_jiffies =
				current_used +
				sys_stat->cpu_idle;
			delta_jiffies =
				current_jiffies - (gdouble)sys_stat->cpu_old_jiffies;

			cpu_usage = (gdouble)( current_used - sys_stat->cpu_old_used ) /
				     (gdouble)delta_jiffies ;
		}
	}
	return cpu_usage;
}

/*
 * configurationfile support
 */
void load_config(void)
{
	XfceRc *rc_file = xfce_rc_simple_open(config_file, FALSE);

	xfce_rc_set_group (rc_file, "General");

	show_user_tasks = xfce_rc_read_bool_entry(rc_file, "show_user_tasks", TRUE);
	show_root_tasks = xfce_rc_read_bool_entry(rc_file, "show_root_tasks", FALSE);
	show_other_tasks = xfce_rc_read_bool_entry(rc_file, "show_other_tasks", FALSE);
	show_cached_as_free = xfce_rc_read_bool_entry(rc_file, "show_cached_as_free", TRUE);

	sort_column = xfce_rc_read_int_entry(rc_file, "sort_column", COLUMN_PID);
	sort_type = xfce_rc_read_int_entry(rc_file, "sort_type", GTK_SORT_ASCENDING);

	win_width = xfce_rc_read_int_entry(rc_file, "win_width", 500);
	win_height = xfce_rc_read_int_entry(rc_file, "win_height", 400);

	xfce_rc_set_group (rc_file, "Information");

	show_col[COLUMN_PID] = xfce_rc_read_bool_entry(rc_file, "show_col_pid", TRUE);
	show_col[COLUMN_PPID] = xfce_rc_read_bool_entry(rc_file, "show_col_ppid", FALSE);
	show_col[COLUMN_STATE] = xfce_rc_read_bool_entry(rc_file, "show_col_state", FALSE);
	show_col[COLUMN_MEM] = xfce_rc_read_bool_entry(rc_file, "show_col_mem", FALSE);
	show_col[COLUMN_RSS] = xfce_rc_read_bool_entry(rc_file, "show_col_rss", TRUE);
	show_col[COLUMN_UNAME] = xfce_rc_read_bool_entry(rc_file, "show_col_uname", FALSE);
	show_col[COLUMN_TIME] = xfce_rc_read_bool_entry(rc_file, "show_col_time", TRUE);
	show_col[COLUMN_PRIO] = xfce_rc_read_bool_entry(rc_file, "show_col_prio", TRUE);

	xfce_rc_close(rc_file);
}

void save_config(void)
{
	XfceRc *rc_file = xfce_rc_simple_open(config_file, FALSE);

	xfce_rc_set_group (rc_file, "General");

	xfce_rc_write_bool_entry(rc_file, "show_user_tasks", show_user_tasks);
	xfce_rc_write_bool_entry(rc_file, "show_root_tasks", show_root_tasks);
	xfce_rc_write_bool_entry(rc_file, "show_other_tasks", show_other_tasks);
	xfce_rc_write_bool_entry(rc_file, "show_cached_as_free", show_cached_as_free);

	gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE(list_store), &sort_column, &sort_type);
	xfce_rc_write_int_entry(rc_file, "sort_column", sort_column);
	xfce_rc_write_int_entry(rc_file, "sort_type", sort_type);

	gtk_window_get_size(GTK_WINDOW (main_window), (gint *) &win_width, (gint *) &win_height);

	xfce_rc_write_int_entry(rc_file, "win_width", win_width);
	xfce_rc_write_int_entry(rc_file, "win_height", win_height);

	xfce_rc_set_group (rc_file, "Information");

	xfce_rc_write_bool_entry(rc_file, "show_col_pid", show_col[COLUMN_PID]);
	xfce_rc_write_bool_entry(rc_file, "show_col_ppid", show_col[COLUMN_PPID]);
	xfce_rc_write_bool_entry(rc_file, "show_col_state", show_col[COLUMN_STATE]);
	xfce_rc_write_bool_entry(rc_file, "show_col_mem", show_col[COLUMN_MEM]);
	xfce_rc_write_bool_entry(rc_file, "show_col_rss", show_col[COLUMN_RSS]);
	xfce_rc_write_bool_entry(rc_file, "show_col_uname", show_col[COLUMN_UNAME]);
	xfce_rc_write_bool_entry(rc_file, "show_col_time", show_col[COLUMN_TIME]);
	xfce_rc_write_bool_entry(rc_file, "show_col_prio", show_col[COLUMN_PRIO]);

	xfce_rc_flush(rc_file);

	xfce_rc_close(rc_file);
}
