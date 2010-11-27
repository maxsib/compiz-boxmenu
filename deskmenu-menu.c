/*
 * compiz-deskmenu is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * compiz-deskmenu is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2008 Christopher Williams <christopherw@verizon.net>
 */
 
 /*
Roadmap:
Necessary:
	TODO: Add a viewport # indicator for the window list for accesiblity reasons difficulty: hard
	TODO: Add configuration for menu icon size difficulty: easy
	TODO: Add toggle of tearables difficulty: easy
	TODO: Add a sane icon dialog difficulty: medium-hard
For fun, might not implement:
TODO: Add ability to call up menus from the menu.xml file by name, if this is really, really needed or requested
 */

#include <stdlib.h>
#include <string.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gtk/gtk.h>

#define HAVE_WNCK 1

#if HAVE_WNCK
#include "deskmenu-wnck.h"
#endif

#include "deskmenu-menu.h"
#include "deskmenu-glue.h"


G_DEFINE_TYPE(Deskmenu, deskmenu, G_TYPE_OBJECT) //this is calling deskmenu_class_init

//launcher.c is the file to look for apwal mode for deskmenu

GQuark
deskmenu_error_quark (void)
{
    static GQuark quark = 0;
    if (!quark)
        quark = g_quark_from_static_string ("deskmenu_error");
    return quark;
}

static void
quit (GtkWidget *widget,
      gpointer   data)
{
    gtk_main_quit ();
}

//stolen from openbox, possibly move this outside in order to make it a function to parse launchers and icon location
static
gchar *parse_expand_tilde(const gchar *f)
{
gchar *ret;
GRegex *regex;

if (!f)
    return NULL;

regex = g_regex_new("(?:^|(?<=[ \\t]))~(?:(?=[/ \\t])|$)",
                    G_REGEX_MULTILINE | G_REGEX_RAW, 0, NULL);
ret = g_regex_replace_literal(regex, f, -1, 0, g_get_home_dir(), 0, NULL);
g_regex_unref(regex);

return ret;
}
//end stolen

//This is how menu command is launched
static void
launcher_activated (GtkWidget *widget,
                    gchar     *command)
{
    GError *error = NULL;
    Deskmenu *deskmenu;
	
    deskmenu = g_object_get_data (G_OBJECT (widget), "deskmenu");
	if (!gdk_spawn_command_line_on_screen (gdk_screen_get_default (), parse_expand_tilde(command), &error))
    {
        GtkWidget *message = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE, "%s", error->message);
        gtk_dialog_run (GTK_DIALOG (message));
        gtk_widget_destroy (message);
    }

}

//This is how a recent document is opened
static void
recent_activated (GtkRecentChooser *chooser,
                  gchar     *command)
{
    GError *error = NULL;
	
	gchar *full_command;
	gchar *file;
	GRegex *regex, *regex2;

	regex = g_regex_new("file:///", G_REGEX_MULTILINE | G_REGEX_RAW, 0, NULL);
	regex2 = g_regex_new("%f", G_REGEX_MULTILINE | G_REGEX_RAW, 0, NULL);

	file = g_strstrip(g_regex_replace_literal(regex, gtk_recent_chooser_get_current_uri (chooser), -1, 0, "/", 0, NULL));
	
	if (g_regex_match (regex2,command,0,0))
	{
		//if using custom complex command, replace %f with filename
		full_command = g_strstrip(g_regex_replace_literal(regex2, command, -1, 0, file, 0, NULL));
	}
	else
	{
		full_command = g_strjoin (" ", command, file, NULL);
	}
	
	g_regex_unref(regex);
	g_regex_unref(regex2);
	
	if (!gdk_spawn_command_line_on_screen (gdk_screen_get_default (), parse_expand_tilde(full_command), &error))
    {
        GtkWidget *message = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE, "%s", error->message);
        gtk_dialog_run (GTK_DIALOG (message));
        gtk_widget_destroy (message);
    }

}

//place & in front of stdout for standard stdout, how a command is launched as an exec
static void
launcher_name_exec_update (GtkWidget *label)
{
    gchar *exec, *stdout;
    exec = g_object_get_data (G_OBJECT (label), "exec");
    if (g_spawn_command_line_sync (parse_expand_tilde(exec), &stdout, NULL, NULL, NULL))
        gtk_label_set_text (GTK_LABEL (label), g_strstrip(stdout));
    else
        gtk_label_set_text (GTK_LABEL (label), "execution error");
    g_free (stdout);
}

static void
deskmenu_construct_item (Deskmenu *deskmenu)
{
    DeskmenuItem *item = deskmenu->current_item;
    GtkWidget *menu_item;
    gchar *name, *icon, *command, *vpicon;
    gint w, h;
//constructs the items in menu
    switch (item->type)
    {
        case DESKMENU_ITEM_LAUNCHER:
            if (item->name_exec)
            {
                GtkWidget *label;
                GHook *hook;

                name = g_strstrip (item->name->str);

                menu_item = gtk_image_menu_item_new ();
                label = gtk_label_new_with_mnemonic (NULL);
                gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

                g_object_set_data (G_OBJECT (label), "exec", g_strdup (name));
                gtk_container_add (GTK_CONTAINER (menu_item), label);
                hook = g_hook_alloc (deskmenu->show_hooks);

                hook->data = (gpointer) label;
                hook->func = (GHookFunc *) launcher_name_exec_update;
                g_hook_append (deskmenu->show_hooks, hook);
            }
            else
            {
                if (item->name)
                    name = g_strstrip (item->name->str);
                else
                    name = "";

                menu_item = gtk_image_menu_item_new_with_mnemonic (name);

            }
            if (item->icon)
            {
                icon = g_strstrip (item->icon->str);
                if (item->icon_file) {
					gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
                	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM
					   (menu_item), gtk_image_new_from_pixbuf (gdk_pixbuf_new_from_file_at_size (parse_expand_tilde(icon), w, h, NULL)));
				   }
				else {
					gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
						gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_MENU));
					}
            }

            if (item->command)
            {
				
                command = g_strstrip (item->command->str);
                g_object_set_data (G_OBJECT (menu_item), "deskmenu", deskmenu);
                g_signal_connect (G_OBJECT (menu_item), "activate",
                    G_CALLBACK (launcher_activated), g_strdup (command));
            }

            gtk_menu_shell_append (GTK_MENU_SHELL (deskmenu->current_menu),
                menu_item);
            break;
#if HAVE_WNCK
        case DESKMENU_ITEM_WINDOWLIST:
            menu_item = gtk_image_menu_item_new_with_mnemonic ("_Windows");

            DeskmenuWindowlist *windowlist = deskmenu_windowlist_new ();
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
                windowlist->menu);
            gtk_menu_shell_append (GTK_MENU_SHELL (deskmenu->current_menu),
                menu_item);
            if (item->icon)
            {
				windowlist->images = TRUE;
                icon = g_strstrip (item->icon->str);
                if (item->icon_file) {
					gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
                	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM
					   (menu_item), gtk_image_new_from_pixbuf (gdk_pixbuf_new_from_file_at_size (parse_expand_tilde(icon), w, h, NULL)));
				}
				else {
					gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
						gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_MENU));
				}
            }
            break;

        case DESKMENU_ITEM_VIEWPORTLIST:
            menu_item = gtk_image_menu_item_new_with_mnemonic ("_Viewports");

            DeskmenuVplist *vplist = deskmenu_vplist_new ();
            if (item->wrap
                && strcmp (g_strstrip (item->wrap->str), "true") == 0)
                vplist->wrap = TRUE;

            gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
                vplist->menu);
            gtk_menu_shell_append (GTK_MENU_SHELL (deskmenu->current_menu),
                menu_item);
            if (item->icon)
            {
				vplist->images = TRUE;
                icon = g_strstrip (item->icon->str);
                if (item->icon_file) {
					gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
                	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM
					   (menu_item), gtk_image_new_from_pixbuf (gdk_pixbuf_new_from_file_at_size (parse_expand_tilde(icon), w, h, NULL)));
				}
				else {
					gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
						gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_MENU));
				}
            }
            if (item->vpicon)
            {
                vpicon = g_strstrip (parse_expand_tilde(item->vpicon->str));
                if (item->vpicon_file) {
					vplist->file = TRUE;
				}
					vplist->icon = vpicon;
            }
            break;
#endif
        case DESKMENU_ITEM_RELOAD:
            menu_item = gtk_image_menu_item_new_with_mnemonic ("Reload");

            if (item->icon)
            {
                icon = g_strstrip (item->icon->str);
                if (item->icon_file) {
					gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
                	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM
					   (menu_item), gtk_image_new_from_pixbuf (gdk_pixbuf_new_from_file_at_size (parse_expand_tilde(icon), w, h, NULL)));
				}
				else {
					gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
						gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_MENU));
				}
            }
            g_signal_connect (G_OBJECT (menu_item), "activate", 
                G_CALLBACK (quit), NULL); 
            gtk_menu_shell_append (GTK_MENU_SHELL (deskmenu->current_menu),
                menu_item);
            break;

        case DESKMENU_ITEM_DOCUMENTS:
            menu_item = gtk_image_menu_item_new_with_mnemonic ("Recent Doc_uments");
            GtkWidget *docs = gtk_recent_chooser_menu_new ();

            if (item->icon)
            {
				gtk_recent_chooser_set_show_icons (GTK_RECENT_CHOOSER(docs), TRUE);
                icon = g_strstrip (item->icon->str);
                if (item->icon_file) {
					gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
                	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM
					   (menu_item), gtk_image_new_from_pixbuf (gdk_pixbuf_new_from_file_at_size (parse_expand_tilde(icon), w, h, NULL)));
				}
				else {
					gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
						gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_MENU));
				}
            }
			else
			{
				gtk_recent_chooser_set_show_icons (GTK_RECENT_CHOOSER(docs), FALSE);
			}
            if (item->age)
            {
				gint days;
				GtkRecentFilter *filter = gtk_recent_filter_new ();
				gtk_recent_filter_add_pattern (filter, "*");
                days = atoi(g_strstrip (item->age->str));
                gtk_recent_filter_add_age (filter, days);
                gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER(docs), filter);
            }
            if (item->sort_type) {
					if (strcmp (g_strstrip (item->sort_type->str), "most used") == 0)
					{
						gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER(docs), GTK_RECENT_SORT_MRU);
					}
					else
					{
						gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER(docs), GTK_RECENT_SORT_LRU);
					}
			}
            if (item->quantity)
            {
				gint limit;
                limit = atoi(g_strstrip (item->quantity->str));
				gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER(docs), limit);
            }
			else
			{
				gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER(docs), -1);
			}
			if (item->command)
			{
				command = g_strstrip (item->command->str);
				g_signal_connect (G_OBJECT (docs), "item-activated",
                    G_CALLBACK (recent_activated), g_strdup (command));
			}
			else
			{
				command = g_strdup ("xdg-open");
				g_signal_connect (G_OBJECT (docs), "item-activated",
                    G_CALLBACK (recent_activated), g_strdup (command));
			}
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
                docs);
            gtk_menu_shell_append (GTK_MENU_SHELL (deskmenu->current_menu),
                menu_item);
            break;
            
        default:
            break;
    }

}
/* The handler functions. */

static void
start_element (GMarkupParseContext *context,
               const gchar         *element_name,
               const gchar        **attr_names,
               const gchar        **attr_values,
               gpointer             user_data,
               GError             **error)
{
    Deskmenu *deskmenu = DESKMENU (user_data);
    DeskmenuElementType element_type;
    const gchar **ncursor = attr_names, **vcursor = attr_values;
    GtkWidget *item, *menu;
    gint w, h;

    element_type = GPOINTER_TO_INT (g_hash_table_lookup
        (deskmenu->element_hash, element_name));

    if ((deskmenu->menu && !deskmenu->current_menu)
       || (!deskmenu->menu && element_type != DESKMENU_ELEMENT_MENU))
    {
        gint line_num, char_num;
        g_markup_parse_context_get_position (context, &line_num, &char_num);
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
            "Error on line %d char %d: Element '%s' declared outside of "
            "toplevel menu element", line_num, char_num, element_name);
        return;
    }

    switch (element_type)
    {
        case DESKMENU_ELEMENT_MENU:

            if (deskmenu->current_item != NULL)
            {
                gint line_num, char_num;
                g_markup_parse_context_get_position (context, &line_num,
                    &char_num);
                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                    "Error on line %d char %d: Element 'menu' cannot be nested "
                    "inside of an item element", line_num, char_num);
                return;
            }
            if (!deskmenu->menu)
            {
	            /*if (strcmp (*ncursor, "size") == 0) {
                    deskmenu->w = g_strdup (*vcursor);
                    deskmenu->h = g_strdup (*vcursor);
                    }
                else {
	                gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, deskmenu->w, deskmenu->h);
                }*/
                deskmenu->menu = gtk_menu_new ();
                g_object_set_data (G_OBJECT (deskmenu->menu), "parent menu",
                    NULL);
                deskmenu->current_menu = deskmenu->menu;
            }
            else
            {
                gchar *name = NULL;
                gchar *icon = NULL;
                gboolean name_exec = FALSE;
                gboolean icon_file = FALSE;
                while (*ncursor)
                {
                    if (strcmp (*ncursor, "name") == 0)
                        name = g_strdup (*vcursor);
                    else if (strcmp (*ncursor, "icon") == 0)
						icon = g_strdup (*vcursor);
                    else if ((strcmp (*ncursor, "mode") == 0)
                        && (strcmp (*vcursor, "exec") == 0))
                        name_exec = TRUE;
                    else if ((strcmp (*ncursor, "mode1") == 0)
                        && (strcmp (*vcursor, "file") == 0))
                        icon_file = TRUE;
					else
                        g_set_error (error, G_MARKUP_ERROR,
                            G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                            "Unknown attribute: %s", *ncursor);
                    ncursor++;
                    vcursor++;
                }
				if (name_exec)
				{
					GtkWidget *label;
					GHook *hook;
				
					item = gtk_image_menu_item_new ();
					label = gtk_label_new_with_mnemonic (NULL);
					gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
				
					g_object_set_data (G_OBJECT (label), "exec", g_strdup (name));
					gtk_container_add (GTK_CONTAINER (item), label);
					hook = g_hook_alloc (deskmenu->show_hooks);
				
					hook->data = (gpointer) label;
					hook->func = (GHookFunc *) launcher_name_exec_update;
					g_hook_append (deskmenu->show_hooks, hook);
				}
				else
				{
					if (name)
						item = gtk_image_menu_item_new_with_mnemonic (name); //allow menus to have icons
				
					else
						item = gtk_image_menu_item_new_with_mnemonic ("");
				}
                if (icon)
				{
					if (icon_file) {
					gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
                	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM
					   (item), gtk_image_new_from_pixbuf (gdk_pixbuf_new_from_file_at_size (parse_expand_tilde(icon), w, h, NULL)));
					}
					else {
					gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
						gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_MENU));
					}
				}
				gtk_menu_shell_append (GTK_MENU_SHELL (deskmenu->current_menu), item);
                menu = gtk_menu_new ();
                g_object_set_data (G_OBJECT (menu), "parent menu",
                    deskmenu->current_menu);
                deskmenu->current_menu = menu;
                gtk_menu_item_set_submenu (GTK_MENU_ITEM (item),
                    deskmenu->current_menu);
                g_free (name);
                g_free (icon);
            }
            break;

        case DESKMENU_ELEMENT_SEPARATOR:
        if (deskmenu->current_item != NULL)
            {
                gint line_num, char_num;
                g_markup_parse_context_get_position (context, &line_num,
                    &char_num);
                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                    "Error on line %d char %d: Element 'menu' cannot be nested "
                    "inside of an item element", line_num, char_num);
                return;
            }
        else {
                gchar *name = NULL;
                gchar *icon = NULL;
                gboolean name_exec = FALSE;
                gboolean icon_file = FALSE;
                gboolean decorate = FALSE;
                gint w, h;
                while (*ncursor)
                {
                    if (strcmp (*ncursor, "name") == 0) {
                        name = g_strdup (*vcursor);
						if (!decorate)
						{
							decorate = TRUE;
						}
					}
                    else if (strcmp (*ncursor, "icon") == 0) {
						icon = g_strdup (*vcursor);
						if (!decorate)
						{
							decorate = TRUE;
						}
					}
                    else if ((strcmp (*ncursor, "mode") == 0)
                        && (strcmp (*vcursor, "exec") == 0))
                        name_exec = TRUE;
                    else if ((strcmp (*ncursor, "mode1") == 0)
                        && (strcmp (*vcursor, "file") == 0))
                        icon_file = TRUE;
					else
                        g_set_error (error, G_MARKUP_ERROR,
                            G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                            "Unknown attribute: %s", *ncursor);
                    ncursor++;
                    vcursor++;
                }
				if (decorate)
				{
					if (name_exec)
					{
						GtkWidget *label;
						GHook *hook;
					
						item = gtk_image_menu_item_new ();
						label = gtk_label_new_with_mnemonic (NULL);
						gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
					
						g_object_set_data (G_OBJECT (label), "exec", g_strdup (name));
						gtk_container_add (GTK_CONTAINER (item), label);
						hook = g_hook_alloc (deskmenu->show_hooks);
					
						hook->data = (gpointer) label;
						hook->func = (GHookFunc *) launcher_name_exec_update;
						g_hook_append (deskmenu->show_hooks, hook);
					}
					else
					{
						item = gtk_image_menu_item_new_with_mnemonic (name);
					}
					if (icon)
					{
						if (icon_file) {
						gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
						gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM
						(item), gtk_image_new_from_pixbuf (gdk_pixbuf_new_from_file_at_size (parse_expand_tilde(icon), w, h, NULL)));
						}
						else {
						gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
							gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_MENU));
						}
					}
					gtk_widget_set_state (item,GTK_STATE_PRELIGHT); /*derive colors from menu hover*/
					gtk_menu_shell_append (GTK_MENU_SHELL (deskmenu->current_menu), item);
					g_free (name);
					g_free (icon);
				}
				else
				{
					item = gtk_separator_menu_item_new ();
					gtk_menu_shell_append (GTK_MENU_SHELL (deskmenu->current_menu), item);
				}
			}
            break;

        case DESKMENU_ELEMENT_ITEM:

            if (deskmenu->current_item != NULL)
            {
                gint line_num, char_num;
                g_markup_parse_context_get_position (context, &line_num,
                    &char_num);
                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                    "Error on line %d char %d: Element 'item' cannot be nested "
                    "inside of another item element", line_num, char_num);
                return;
            }

            deskmenu->current_item = g_slice_new0 (DeskmenuItem);
                while (*ncursor)
                {
                    if (strcmp (*ncursor, "type") == 0)
                        deskmenu->current_item->type = GPOINTER_TO_INT
                        (g_hash_table_lookup (deskmenu->item_hash, *vcursor));
                    else
                        g_set_error (error, G_MARKUP_ERROR,
                            G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                            "Unknown attribute: %s", *ncursor);
                    ncursor++;
                    vcursor++;
                }
            break;

        case DESKMENU_ELEMENT_NAME:
             while (*ncursor)
                {
                    if ((strcmp (*ncursor, "mode") == 0)
                        && (strcmp (*vcursor, "exec") == 0))
                        deskmenu->current_item->name_exec = TRUE;
                    ncursor++;
                    vcursor++;
                } /* no break here to let it fall through */
        case DESKMENU_ELEMENT_ICON:
                while (*ncursor)
                {
                    if ((strcmp (*ncursor, "mode1") == 0)
                        && (strcmp (*vcursor, "file") == 0))
                        deskmenu->current_item->icon_file = TRUE;
                    ncursor++;
                    vcursor++;
                } /* no break here to let it fall through */
        case DESKMENU_ELEMENT_VPICON:
                while (*ncursor)
                {
                    if ((strcmp (*ncursor, "mode1") == 0)
                        && (strcmp (*vcursor, "file") == 0))
                        deskmenu->current_item->vpicon_file = TRUE;
                    ncursor++;
                    vcursor++;
                } /* no break here to let it fall through */
        case DESKMENU_ELEMENT_COMMAND:
        case DESKMENU_ELEMENT_WRAP:
            if (deskmenu->current_item)
                deskmenu->current_item->current_element = element_type;
            break;
        case DESKMENU_ELEMENT_QUANTITY:
            if (deskmenu->current_item)
                deskmenu->current_item->current_element = element_type;
            break;
        case DESKMENU_ELEMENT_SORT:
            if (deskmenu->current_item)
                deskmenu->current_item->current_element = element_type;
            break;
        case DESKMENU_ELEMENT_AGE:
            if (deskmenu->current_item)
                deskmenu->current_item->current_element = element_type;
            break;
        default:
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                "Unknown element: %s", element_name);
            break;
    }
}
//dealing with empty attributes
static void
text (GMarkupParseContext *context,
      const gchar         *text,
      gsize                text_len,
      gpointer             user_data,
      GError             **error)
{
    Deskmenu *deskmenu = DESKMENU (user_data);
    DeskmenuItem *item = deskmenu->current_item;

    if (!(item && item->current_element))
        return;

    switch (item->current_element)
    {
        case DESKMENU_ELEMENT_NAME:
            if (!item->name)
                item->name = g_string_new_len (text, text_len);
            else
                g_string_append_len (item->name, text, text_len);
            break;

        case DESKMENU_ELEMENT_ICON:
            if (!item->icon)
                item->icon = g_string_new_len (text, text_len);
            else
                g_string_append_len (item->icon, text, text_len);
            break;

        case DESKMENU_ELEMENT_VPICON:
            if (!item->vpicon)
                item->vpicon = g_string_new_len (text, text_len);
            else
                g_string_append_len (item->vpicon, text, text_len);
            break;

        case DESKMENU_ELEMENT_COMMAND:
            if (!item->command)
                item->command = g_string_new_len (text, text_len);
            else
                g_string_append_len (item->command, text, text_len);
            break;

        case DESKMENU_ELEMENT_WRAP:
            if (!item->wrap)
                item->wrap = g_string_new_len (text, text_len);
            else
                g_string_append_len (item->wrap, text, text_len);
            break;

        case DESKMENU_ELEMENT_AGE:
            if (!item->age)
                item->age = g_string_new_len (text, text_len);
            else
                g_string_append_len (item->age, text, text_len);
            break;

        case DESKMENU_ELEMENT_QUANTITY:
            if (!item->quantity)
                item->quantity = g_string_new_len (text, text_len);
            else
                g_string_append_len (item->quantity, text, text_len);
            break;

        case DESKMENU_ELEMENT_SORT:
            if (!item->sort_type)
                item->sort_type = g_string_new_len (text, text_len);
            else
                g_string_append_len (item->sort_type, text, text_len);
            break;

        default:
            break;
    }
}

static void
end_element (GMarkupParseContext *context,
             const gchar         *element_name,
             gpointer             user_data,
             GError             **error)
{

    DeskmenuElementType element_type;
    Deskmenu *deskmenu = DESKMENU (user_data);
    GtkWidget *parent;
    element_type = GPOINTER_TO_INT (g_hash_table_lookup
        (deskmenu->element_hash, element_name));

    switch (element_type)
    {
        case DESKMENU_ELEMENT_MENU:

            g_return_if_fail (deskmenu->current_item == NULL);

            parent = g_object_get_data (G_OBJECT (deskmenu->current_menu),
                "parent menu");

            deskmenu->current_menu = parent;

            break;
/* separator building is now dealt with in the beginning */
        case DESKMENU_ELEMENT_ITEM:

            g_return_if_fail (deskmenu->current_item != NULL);

            /* finally make the item ^_^ */
            deskmenu_construct_item (deskmenu);

            /* free data used to make it */
            if (deskmenu->current_item->name)
                g_string_free (deskmenu->current_item->name, TRUE);
            if (deskmenu->current_item->icon)
                g_string_free (deskmenu->current_item->icon, TRUE);
            if (deskmenu->current_item->command)
                g_string_free (deskmenu->current_item->command, TRUE);
            if (deskmenu->current_item->wrap)
                g_string_free (deskmenu->current_item->wrap, TRUE);
            if (deskmenu->current_item->vpicon)
                g_string_free (deskmenu->current_item->vpicon, TRUE);
            if (deskmenu->current_item->sort_type)
                g_string_free (deskmenu->current_item->sort_type, TRUE);
            if (deskmenu->current_item->quantity)
                g_string_free (deskmenu->current_item->quantity, TRUE);
            if (deskmenu->current_item->age)
                g_string_free (deskmenu->current_item->age, TRUE);
            g_slice_free (DeskmenuItem, deskmenu->current_item);
            deskmenu->current_item = NULL;
            break;
        default:
            break;
    }
}

/* The list of what handler does what. */
//this parses menus
static GMarkupParser parser = {
    start_element,
    end_element,
    text,
    NULL,
    NULL
};


/* Class init */
static void 
deskmenu_class_init (DeskmenuClass *deskmenu_class)
{
    dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (deskmenu_class),
        &dbus_glib_deskmenu_object_info);
}

/* Instance init, matches up words to types, note how there's no handler for pipe since it's replaced in its own chunk */
static void 
deskmenu_init (Deskmenu *deskmenu)
{

    deskmenu->show_hooks = g_slice_new0 (GHookList);

    g_hook_list_init (deskmenu->show_hooks, sizeof (GHook));


    deskmenu->menu = NULL;
    deskmenu->current_menu = NULL;
    deskmenu->current_item = NULL;

    deskmenu->item_hash = g_hash_table_new (g_str_hash, g_str_equal);

    g_hash_table_insert (deskmenu->item_hash, "launcher",
        GINT_TO_POINTER (DESKMENU_ITEM_LAUNCHER));
#if HAVE_WNCK
    g_hash_table_insert (deskmenu->item_hash, "windowlist",
        GINT_TO_POINTER (DESKMENU_ITEM_WINDOWLIST));
    g_hash_table_insert (deskmenu->item_hash, "viewportlist",
        GINT_TO_POINTER (DESKMENU_ITEM_VIEWPORTLIST));
#endif
    g_hash_table_insert (deskmenu->item_hash, "documents",
        GINT_TO_POINTER (DESKMENU_ITEM_DOCUMENTS));
    g_hash_table_insert (deskmenu->item_hash, "reload",
        GINT_TO_POINTER (DESKMENU_ITEM_RELOAD));

    deskmenu->element_hash = g_hash_table_new (g_str_hash, g_str_equal);
    
    g_hash_table_insert (deskmenu->element_hash, "menu", 
        GINT_TO_POINTER (DESKMENU_ELEMENT_MENU));
    g_hash_table_insert (deskmenu->element_hash, "separator",
        GINT_TO_POINTER (DESKMENU_ELEMENT_SEPARATOR));
    g_hash_table_insert (deskmenu->element_hash, "item", 
        GINT_TO_POINTER (DESKMENU_ELEMENT_ITEM));
    g_hash_table_insert (deskmenu->element_hash, "name", 
        GINT_TO_POINTER (DESKMENU_ELEMENT_NAME));
    g_hash_table_insert (deskmenu->element_hash, "icon", 
        GINT_TO_POINTER (DESKMENU_ELEMENT_ICON));
    g_hash_table_insert (deskmenu->element_hash, "vpicon", 
        GINT_TO_POINTER (DESKMENU_ELEMENT_VPICON));
    g_hash_table_insert (deskmenu->element_hash, "command", 
        GINT_TO_POINTER (DESKMENU_ELEMENT_COMMAND));
    g_hash_table_insert (deskmenu->element_hash, "wrap", 
        GINT_TO_POINTER (DESKMENU_ELEMENT_WRAP));
    g_hash_table_insert (deskmenu->element_hash, "sort", 
        GINT_TO_POINTER (DESKMENU_ELEMENT_SORT));
    g_hash_table_insert (deskmenu->element_hash, "quantity", 
        GINT_TO_POINTER (DESKMENU_ELEMENT_QUANTITY));
    g_hash_table_insert (deskmenu->element_hash, "age", 
        GINT_TO_POINTER (DESKMENU_ELEMENT_AGE));
}

//http://www.ibm.com/developerworks/linux/tutorials/l-glib/section5.html
static
gchar *check_file_cache (gchar *filename) {
static GHashTable *cache;
gchar *t = NULL;
gchar *f = NULL;
gchar *user_default = g_build_path (G_DIR_SEPARATOR_S,  g_get_user_config_dir (),
                                   "compiz",
                                   "deskmenu",
                                   "menu.xml",
                                   NULL);

//TODO: add a size column to cache
if (!cache)
{
	g_print("Creating cache...");
	cache = g_hash_table_new (g_str_hash, g_str_equal);
	g_print("Done creating cache!\n");
}
else
{
	g_print("Checking cache...\n");	
}

if (strlen(filename) == 0) {
	g_print("No filename supplied, looking up default menu...\n");
		/*
        set default filename to be [configdir]/compiz/deskmenu/menu.xml
        */
        filename = user_default;
    if (!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
			g_print("Getting default system menu...");
			const gchar* const *cursor = g_get_system_config_dirs ();
			gchar *path = NULL;
			while (*cursor)
			{
				g_free (path);
				path = g_strdup (*cursor);
				filename = g_build_path (G_DIR_SEPARATOR_S,
										path,
										"compiz",
										"deskmenu",
										"menu.xml",
										NULL);
		
				if (g_file_get_contents (filename, &f, NULL, NULL))
				{
					g_hash_table_replace (cache, g_strdup(filename), g_strdup(f));
					g_free (path);
					g_print("Got it!");
					break;
				}
					cursor++;
			}
		}
	else
		{
			if (!g_hash_table_lookup_extended(cache, user_default, NULL, NULL))
			{
				g_file_get_contents (filename, &f, NULL, NULL);
				g_print("Cacheing default user file...\n");
				g_hash_table_replace (cache, g_strdup(filename), g_strdup(f));
			}
			g_print("Retrieving cached default user file...\n");
		}
}
else 
{
	if (!g_hash_table_lookup_extended(cache, filename, NULL, NULL)) {
		if (g_file_get_contents (filename, &f, NULL, NULL))
			{
				g_print("Cacheing new non-default file...\n");
				g_hash_table_replace (cache, g_strdup(filename), g_strdup(f));
			}
			else {
				if (g_hash_table_lookup_extended(cache, user_default, NULL, NULL)) //do if both exist, again, doesn't work
				{
					g_print("Couldn't find specified file, loading default...\n");
					filename = user_default;
				}
				else
				{
					g_printerr ("Couldn't find a menu file...\n");
					exit (1);
				}
			}
	}
}

t = g_hash_table_lookup (cache, filename);
g_printf("Done loading %s!\n", filename);
g_free (f);
g_free (filename);
return t;
}

static void
deskmenu_parse_text (Deskmenu *deskmenu, gchar *text)
{
    GError *error = NULL;
    gchar *exec, *stdout, *pipe_error;
	GRegex *regex, *command;
	int i = 0;

    GMarkupParseContext *context = g_markup_parse_context_new (&parser,
        0, deskmenu, NULL);
    
	pipe_error = g_strdup ("<item type=\"launcher\"><name>Cannot retrieve pipe output</name></item>");
	regex = g_regex_new("(<pipe command=\".*\"/>)", G_REGEX_MULTILINE | G_REGEX_RAW, 0, NULL);
	command = g_regex_new("<pipe command=\"|\"/>", G_REGEX_MULTILINE | G_REGEX_RAW, 0, NULL);
	gchar **menu_chunk = g_regex_split (regex, text, 0); //this splits the menu into parsable chunks, needed for pipe item capabilities
	
	//this loop will replace the pipeitem chunk with its output, other chunks are let through as is
	while (menu_chunk[i])
	{
		if (g_regex_match (regex,menu_chunk[i],0,0))
		{
            exec = parse_expand_tilde(g_strstrip(g_regex_replace_literal(command, menu_chunk[i], -1, 0, "", 0, NULL)));
			if (g_spawn_command_line_sync (exec, &stdout, NULL, NULL, NULL))
			{
				menu_chunk[i] = stdout;
			}
			else
			{
				menu_chunk[i] = pipe_error;
			}
            //fix this to be able to replace faulty pipe with an item that says it can't be executed
            //needs validator in order to make sure it can be parsed, if not, set parsed error
		}
		i++;
	}
	text = g_strjoinv (NULL, menu_chunk); //stitch the text so we can get error reporting back to normal
	
	if (!g_markup_parse_context_parse (context, text, strlen(text), &error)
        || !g_markup_parse_context_end_parse (context, &error))
    {
        g_printerr ("Parse failed with message: %s \n", error->message);
        g_error_free (error);
        exit (1);
    }

	g_free(text); //free the joined array
	g_strfreev(menu_chunk); //free the menu chunks and their container
	g_regex_unref(regex); //free the pipeitem chunk checker
	g_regex_unref(command); //free the pipe command extractor
    g_markup_parse_context_free (context); //free the parser

    gtk_widget_show_all (deskmenu->menu);
}

/* The show method */
static void
deskmenu_show (Deskmenu *deskmenu,
               GError  **error)
{
    g_hook_list_invoke (deskmenu->show_hooks, FALSE);

    gtk_menu_popup (GTK_MENU (deskmenu->menu),
                    NULL, NULL, NULL, NULL,
                    0, 0);
}

gboolean
deskmenu_reload (Deskmenu *deskmenu,
               GError  **error)
{
    gtk_main_quit ();
    //this will destroy all data the daemon currently gathered
    return TRUE;
}

/* The dbus method */
gboolean
deskmenu_control (Deskmenu *deskmenu, gchar *filename, GError  **error)
{
	if (deskmenu->menu)
	{
		deskmenu->menu = NULL; //destroy the current menu so we can refresh it
	}
	deskmenu_parse_text(deskmenu, check_file_cache(g_strdup(filename))); //recreate the menu, check caches for data
	deskmenu_show(deskmenu, error);
	return TRUE;
}

/*
//precache backend, currently needs GUI
static void
deskmenu_precache (gchar *filename)
{
	GError *error = NULL;
	GKeyfile *config = g_key_file_new ();
	gboolean success = FALSE;
	
	g_print("Attempting to precache files in config...");
	if (!filename)
	{
		filename = g_build_path (G_DIR_SEPARATOR_S,
                                   g_get_user_config_dir (),
                                   "compiz",
                                   "deskmenu",
                                   "precache.ini",
                                   NULL);
	}
	if (!g_key_file_load_from_file (config, filename, G_KEY_FILE_NONE, NULL))
	{
		g_print("Configuration not found, will not precache files...");
		return;
	}
	else
	{
		g_print("Configuration found! Starting precache...");
	}
	
	gchar **files = g_key_file_get_keys (config, "File", NULL, &error);
	gchar *feed;
//format
* [Files]
* file_1=/path/to/bla.xml
* file_2=/path/to/bla.xml
	while (files[i])
	{
		feed = g_strstrip(g_key_file_get_value (config, "File", files[i], &error));
		deskmenu_check_file_cache(feed);
	}
	g_strfreev(files);
	g_free(feed);
	g_key_file_free (config);
}*/
//http://library.gnome.org/devel/glib/stable/glib-Key-value-file-parser.html#g-key-file-get-string

int
main (int    argc,
      char **argv)
{
	DBusGConnection *connection;
    GError *error = NULL;
    GObject *deskmenu;

    g_type_init ();
 
    connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (connection == NULL)
    {
        g_printerr ("Failed to open connection to bus: %s", error->message);
        g_error_free (error);
        exit (1);
    }
//gtk_tearoff_menu_item_new();
	g_print ("Starting the daemon...\n");
	/*
	GOptionContext *context;
	gchar *file;
    GOptionEntry entries[] =
    {
        { "config", 'c', 0, G_OPTION_ARG_FILENAME, &file,
            "Use FILE instead of the default daemon configuration", "FILE" },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
    };

    context = g_option_context_new (NULL);
    g_option_context_add_main_entries (context, entries, NULL);
	error = NULL;
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_printerr ("option parsing failed: %s", error->message);
        g_error_free (error);
        return 1;
    }	
	g_option_context_free (context);
	deskmenu_precache(file);
	*/
#if HAVE_WNCK
    wnck_set_client_type (WNCK_CLIENT_TYPE_PAGER);
#endif

    gtk_init (&argc, &argv);
	
    deskmenu = g_object_new (DESKMENU_TYPE, NULL);

    dbus_g_connection_register_g_object (connection, DESKMENU_PATH_DBUS, deskmenu);

	if (!dbus_bus_request_name (dbus_g_connection_get_connection (connection),
						        DESKMENU_SERVICE_DBUS,
                                DBUS_NAME_FLAG_REPLACE_EXISTING,
						        NULL))
        return 1;

    gtk_main ();

    return 0;
}
