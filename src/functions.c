/*
 *  xfce4-taskmanager - very simple taskmanger
 *
 *  Copyright (c) 2005 Johannes Zellner, <webmaster@nebulon.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "functions.h"

gboolean refresh_task_list(void)
{
	DIR *dir;

	/* load the /proc dir */
	if((dir = opendir(PROC_DIR_1)) == NULL)
	{
		if((dir = opendir(PROC_DIR_2)) == NULL)
		{
			if((dir = opendir(PROC_DIR_3)) == NULL)
			{
				fprintf(stderr, "Warning: couldn't load the /proc directory\n");
				return FALSE;
			}
		}
	}
			
	struct dirent *dir_entry;
	gint i;
		
	/* markes all tasks to "not checked" */
	for(i = 0; i < tasks; i++)
	{
		struct task *tmp = &g_array_index(task_array, struct task, i);
		tmp->checked = FALSE;
	}

	while((dir_entry = readdir(dir)) != NULL)
	{
		if(atoi(dir_entry->d_name) != 0)
		{
			FILE *task_file_status;
			
			gchar task_file_name_status[64] = "/proc/";
			g_strlcat(task_file_name_status,dir_entry->d_name, sizeof task_file_name_status);
			g_strlcat(task_file_name_status,"/status", sizeof task_file_name_status);
		
			gchar buffer_status[256];
			struct task task;
			struct passwd *passwdp;
					
			if((task_file_status = fopen(task_file_name_status,"r")) != NULL)
			{				
				while(fgets(buffer_status, sizeof buffer_status, task_file_status) != NULL)
				{
					sscanf(buffer_status,"Uid: %i",&task.uid);
					sscanf(buffer_status,"Pid: %i",&task.pid);
					sscanf(buffer_status,"PPid: %i",&task.ppid);
					sscanf(buffer_status,"Name: %s",&task.name);
					sscanf(buffer_status,"VmSize: %i",&task.size);
					sscanf(buffer_status,"VmRSS: %i",&task.rss);
					// fix for freebsd with linux emo
					sscanf(buffer_status,"VmRss: %i",&task.rss);
					sscanf(buffer_status,"State: %c",&task.state);
				}
					
				passwdp = getpwuid(task.uid);
				if(passwdp != NULL && passwdp->pw_name != NULL)
					g_strlcpy(task.uname, passwdp->pw_name, sizeof task.uname);
			}
			
			fclose(task_file_status);
				
			/* check if task is new and marks the task that its checked*/
			gboolean new_task = TRUE;
				
			if(tasks < 1)
			{
				task_array = g_array_append_val(task_array, task);
				tasks++;
			}
				
			for(i = 0; i < tasks; i++)
			{
				struct task *data = &g_array_index(task_array, struct task, i);
					
				if((gint)data->pid == task.pid)
				{
					if((gint)data->ppid != task.ppid || (gchar)data->state != task.state || (unsigned int)data->size != task.size || (unsigned int)data->rss != task.rss)
					{
						data->ppid = task.ppid;
						data->state = task.state;
						data->size = task.size;
						data->rss = task.rss;
						
						refresh_list_item(i);
					}
					new_task = FALSE;
					data->checked = TRUE;
				}
			}
				
			if(new_task)
			{
				g_array_append_val(task_array, task);
				tasks++;
				if((show_user_tasks && task.uid == own_uid) || (show_root_tasks && task.uid == 0) ||  (show_other_tasks && task.uid != own_uid && task.uid != 0))
					add_new_list_item(tasks-1);
			}
		}
	}
	closedir(dir);
	
	/* removing all tasks, which are not "checked" */
	for(i = 0; i < tasks; i++)
	{
		struct task *data = &g_array_index(task_array, struct task, i);
					
		if(!data->checked)
		{
			remove_list_item((gint)data->pid);
			task_array = g_array_remove_index(task_array, i);
			tasks--;
		}
	}
	
	return TRUE;
}

void fill_list_item(gint i, GtkTreeIter *iter)
{
	if(iter != NULL)
	{
		struct task *task = &g_array_index(task_array, struct task, i);
		
		gchar *pid = g_strdup_printf("%i", task->pid);
		gchar *ppid = g_strdup_printf("%i", task->ppid);
		gchar *state = g_strdup_printf("%c", task->state);
		gchar *size = g_strdup_printf("%i kb", task->size);
		gchar *rss = g_strdup_printf("%i kb", task->rss);
		gchar *name = g_strdup_printf("%s", task->name);
		gchar *uname = g_strdup_printf("%s", task->uname);
	
		gtk_list_store_set(GTK_LIST_STORE(list_store), iter, 0, name, -1);
		gtk_list_store_set(GTK_LIST_STORE(list_store), iter, 1, pid, -1);
		gtk_list_store_set(GTK_LIST_STORE(list_store), iter, 2, ppid, -1);
		gtk_list_store_set(GTK_LIST_STORE(list_store), iter, 3, state, -1);
		gtk_list_store_set(GTK_LIST_STORE(list_store), iter, 4, size, -1);
		gtk_list_store_set(GTK_LIST_STORE(list_store), iter, 5, rss, -1);
		gtk_list_store_set(GTK_LIST_STORE(list_store), iter, 6, uname, -1);
		
		free(pid);
		free(ppid);
		free(state);
		free(size);
		free(rss);
		free(name);
		free(uname);
	}
}

void add_new_list_item(gint i)
{
		GtkTreeIter iter;
	
		gtk_list_store_append(GTK_LIST_STORE(list_store), &iter);
	
		fill_list_item(i, &iter);
}

void refresh_list_item(gint i)
{
	GtkTreeIter iter;
	
	gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list_store), &iter);
	
	struct task task = g_array_index(task_array, struct task, i);
		
	while(valid)
	{
		gchar *str_data = "";
		gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, 1, &str_data, -1);

		if(task.pid == atoi(str_data))
		{
			g_free(str_data);
			fill_list_item(i, &iter);
			break;
		}

		g_free(str_data);
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store), &iter);
	}
}

void remove_list_item(gint pid)
{
	GtkTreeIter iter;
	
	gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list_store), &iter);
	
	while(valid)
	{
		gchar *str_data = "";
		gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, 1, &str_data, -1);

		if(pid == atoi(str_data))
		{
			free(str_data);
			gtk_list_store_remove(GTK_LIST_STORE(list_store), &iter);
			break;
		}

		free(str_data);
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store), &iter);
	}
}

gint compare_list_item(GtkTreeModel *model, GtkTreeIter *iter1, GtkTreeIter *iter2, gpointer colnum)
{	
	gchar *s1 = "";
	gchar *s2 = "";
	gint ret = 0;

	gtk_tree_model_get(model, iter1, colnum, &s1, -1);
	gtk_tree_model_get(model, iter2, colnum, &s2, -1);

	if((gint)colnum == 1 || (gint)colnum == 2 || (gint)colnum == 4 || (gint)colnum == 5)
	{
		gint i1 = 0;
		gint i2 = 0;
	
		if(s1 != NULL)
			i1 = atoi(s1);
		
		if(s2 != NULL)
			i2 = atoi(s2);
	
		if(i1 < i2)
			ret = -1;
		else
			ret = 1;
	}
	else
	{
		if(s1 == NULL)
			s1 = "";
		if(s2 == NULL)
			s2 = "";
	
		ret = strcmp(s1, s2);
	}
	
	free(s1);
	free(s2);
	
	return ret;
}

/* function to send the signal to the current task */
void send_signal_to_task(gchar *task_id, gchar *signal)
{
	if(task_id != "" && signal != NULL)
	{
		gchar command[64] = "kill -";
		g_strlcat(command,signal, sizeof command);
		g_strlcat(command," ", sizeof command);
		g_strlcat(command,task_id, sizeof command);
		
		if(system(command) != 0)
			xfce_err("Couldn't %s the task with ID %s", signal, task_id);
	}
}

/* change the task view (user, root, other) */
void change_task_view(void)
{
	gtk_list_store_clear(GTK_LIST_STORE(list_store));
	
	gint i = 0;
	
	for(i = 0; i < tasks; i++)
	{
		struct task task = g_array_index(task_array, struct task, i);
		
		if((task.uid == own_uid && show_user_tasks) || (task.uid == 0 && show_root_tasks) || (task.uid != own_uid && task.uid != 0 && show_other_tasks))
			add_new_list_item(i);
	}
	
	refresh_task_list();
}