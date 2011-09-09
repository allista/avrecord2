/***************************************************************************
 *   Copyright (C) 2007 by Allis Tauri   *
 *   allista@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <gtk/gtk.h>

void
on_window_destroy (GtkObject *object, gpointer user_data)
{
	gtk_main_quit();
}

int main (int argc, char *argv[])
{
	GtkBuilder              *builder;
	GtkWidget               *window;

	gtk_init (&argc, &argv);

	builder = gtk_builder_new ();
	gtk_builder_add_from_file (builder, "gtkapp.glade", NULL);

	window = GTK_WIDGET (gtk_builder_get_object (builder, "MainWindow"));
	gtk_builder_connect_signals (builder, NULL);
	g_object_unref (G_OBJECT (builder));

	gtk_widget_show (window);
	gtk_main ();

	return 0;
}