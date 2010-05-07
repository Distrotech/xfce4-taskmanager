/*
 * Copyright (c) 2005-2006 Johannes Zellner, <webmaster@nebulon.de>
 * Copyright (c) 2010 Mike Massonnet, <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "process-window.h"
#include "task-manager.h"

static GtkWidget *window;
static XtmTaskManager *task_manager;
static gboolean timeout = 0;

static gboolean
init_timeout ()
{
	guint num_processes;
	gfloat cpu, memory, swap;
	const GArray *task_list;

	xtm_task_manager_get_system_info (task_manager, &num_processes, &cpu, &memory, &swap);
	xtm_process_window_set_system_info (XTM_PROCESS_WINDOW (window), num_processes, cpu, memory, swap);

	xtm_task_manager_update_model (task_manager);

	if (timeout == 0)
		timeout = g_timeout_add (1000, init_timeout, NULL);

	return TRUE;
}

int main (int argc, char *argv[])
{
#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	gtk_init (&argc, &argv);

	window = xtm_process_window_new ();
	gtk_widget_show (window);

	task_manager = xtm_task_manager_new (xtm_process_window_get_model (XTM_PROCESS_WINDOW (window)));
	g_message ("Running as %s on %s", xtm_task_manager_get_username (task_manager), xtm_task_manager_get_hostname (task_manager));

	init_timeout ();

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_main ();

	if (timeout > 0)
		g_source_remove (timeout);
	g_object_unref (window);

	return 0;
}

