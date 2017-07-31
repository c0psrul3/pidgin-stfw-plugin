/*
 * stfw-plugin - automatically replies to questions using lmgtfy.com
 * It uses small parts of the autoreply-plugin by Sadrul Habib
 * Chowdhury <sadrul@users.sourceforge.net>. 
 *
 * Copyright (C) 2009
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

#include <glib.h>
#ifdef _WIN32
# include <win32dep.h>
#endif
#define PURPLE_PLUGINS
#include <version.h>

#define PLUGIN_ID			"core-Kim_Stebel-STFW_Autoreply"
#define PLUGIN_STATIC_NAME	"STFW"
#define PLUGIN_AUTHOR		"Kim Stebel <kim.stebel@gmail.com"

#define	PREFS_PREFIX		"/plugins/core/" PLUGIN_ID
#define	PREFS_AUTO			PREFS_PREFIX "/auto"
#define	PREFS_BUDDIES		PREFS_PREFIX "/buddies"

#include <util.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <debug.h>
#include "entities.h"
#include <request.h>
#include <cmds.h>

static const char c2x_table[] = "0123456789ABCDEF";
PurplePlugin *plugin_handle = NULL;

char *get_reply_from_query(char *query)
{
	char *m_without_html = (char*)malloc(strlen(query)+1);
	m_without_html = purple_markup_strip_html(query);
	decode_html_entities_utf8(m_without_html, NULL);
	purple_debug_misc(PLUGIN_ID, "without_html = %s\n", m_without_html);
	const char *buffer = purple_url_encode(m_without_html);
	purple_debug_misc(PLUGIN_ID, "encoded = %s\n", buffer);
	g_free(m_without_html);
	char *buffer2 = (char*)malloc(strlen(buffer) + 30);		
	strcpy(buffer2, "http://lmgtfy.com/?q=");
	strcat(buffer2, buffer);
	return buffer2;
}

static const char *
get_autoreply_message(PurpleBuddy *buddy, PurpleAccount *account, const char *message)
{
	const char *reply = NULL;
	int question = FALSE;
	int is_on_list = FALSE;
	char *mp;
	for (mp = (char*)message; *mp != '\0'; mp++)
	{
		if (*mp=='?')
		{
			question = TRUE;
			break;
		}
	}
	if (!question) return FALSE;
	
	purple_debug_misc(PLUGIN_ID, "message = %s\n", message);
	int autoreply = purple_prefs_get_bool(PREFS_AUTO);
	if (!autoreply)
	{
		return NULL;
	}
	const char *buddies = purple_prefs_get_string("/plugins/core/core-Kim_Stebel-STFW_Autoreply/buddies");
	g_strstrip((char *)buddies);
	if (!(!buddies || !(*buddies)))
	{
		const char *name = purple_buddy_get_name(buddy);
		purple_debug_misc(PLUGIN_ID, "buddy name = %s\n", name);
		if ((name != NULL) && (strstr(buddies, name) != NULL))
		{
			is_on_list = TRUE;
		}
		if (!strcmp("all", buddies))
		{
			is_on_list = TRUE;
		}
	}
	if (question && is_on_list)
	{
		reply = get_reply_from_query((char*)message);
	}
	return reply;
}

static void
written_msg(PurpleAccount *account, const char *who, const char *message,
				PurpleConversation *conv, PurpleMessageFlags flags, gpointer null)
{
	PurpleBuddy *buddy;
	const char *reply = NULL;

	if (!(flags & PURPLE_MESSAGE_RECV))
		return;

	if (!message || !*message)
		return;

	/* Do not send an autoreply for an autoreply */
	if (flags & PURPLE_MESSAGE_AUTO_RESP)
		return;

	if (purple_conversation_get_type(conv) != PURPLE_CONV_TYPE_IM)
		return;

	buddy = purple_find_buddy(account, who);
	reply = get_autoreply_message(buddy, account, message);

	if (reply)
	{
		PurpleConnection *gc;
		PurpleMessageFlags flag = PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_AUTO_RESP;
		time_t now;
		
		now = time(NULL);

		/*actually send the message*/
		gc = purple_account_get_connection(account);
		serv_send_im(gc, who, reply, flag);
		purple_conv_im_write(PURPLE_CONV_IM(conv), NULL, reply, flag, time(NULL));
		free((void*)reply);
		
	}
}

void send_message(PurpleConversation *conv, gchar *message)
{
	if (message)
	{
		purple_debug_misc(PLUGIN_ID, "sending message: %s\n", message);
		PurpleConnection *gc;
		PurpleMessageFlags flag = PURPLE_MESSAGE_SEND;
		time_t now;
		now = time(NULL);
		PurpleAccount *account;
		account = purple_conversation_get_account(conv);
		gc = purple_account_get_connection(account);
		purple_debug_misc(PLUGIN_ID, "preparation done: %s\n", message);
		serv_send_im(gc, conv->name, message, flag);
		purple_conv_im_write(PURPLE_CONV_IM(conv), NULL, message, flag, time(NULL));
		purple_debug_misc(PLUGIN_ID, "message sent: %s\n", message);
	}
}

PurpleCmdRet stfw_s_cb(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	purple_debug_misc(PLUGIN_ID, "stfw_s_cb called\n");
	char *reply = NULL;
	reply = get_reply_from_query(args[0]);
	send_message(conv, reply);
	free(reply);
	return PURPLE_CMD_RET_OK;
}

char *last_message_received(PurpleConversation *conv)
{
	GList* list;
	list = purple_conversation_get_message_history(conv);
	if (!list)
	{
		purple_debug_misc(PLUGIN_ID, "conv history empty");
		return "";
	}
	while ((list = list->next)) /*still messages in list*/
	{
		PurpleConvMessage *pcm = list->data;
		purple_debug_misc(PLUGIN_ID, "message: %s sender:%s\n", pcm->what, pcm->who);
		if (!strcmp(pcm->who, conv->name) && !strcmp(pcm->alias, conv->name))
		{
			return pcm->what;
		}
		
	}
	return NULL;
}

PurpleCmdRet stfw_cb(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	purple_debug_misc(PLUGIN_ID, "stfw_cb called\n");
	char *reply;
	reply = get_reply_from_query(last_message_received(conv));
	if (!reply) return PURPLE_CMD_RET_FAILED;
	send_message(conv, reply);
	free(reply);
	return PURPLE_CMD_RET_OK;
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_signal_connect(purple_conversations_get_handle(), "wrote-im-msg", plugin,
						PURPLE_CALLBACK(written_msg), NULL);
	plugin_handle = plugin;
	purple_cmd_register	(
	 	"stfw", /*command name*/
		"s", /*args*/
		0, /*priority*/
		PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, /*flags*/
		NULL, /*prpl id not needed*/
		stfw_s_cb, /*callback function*/
		"send a link to lmgtfy with the argument as the query string", /*help string*/
		NULL /* user defined data not needed*/	 
	);
	purple_cmd_register	(
	 	"lmgtfy", /*command name*/
		"s", /*args*/
		0, /*priority*/
		PURPLE_CMD_FLAG_IM, /*flags*/
		NULL, /*prpl id not needed*/
		stfw_s_cb, /*callback function*/
		"send a link to lmgtfy with the argument as the query string", /*help string*/
		NULL /* user defined data not needed*/	 
	);
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	return TRUE;
}

/*prefs*/

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *pref;

	frame = purple_plugin_pref_frame_new();

	pref = purple_plugin_pref_new_with_name_and_label(PREFS_AUTO,
					"Automatically reply to questions with a link to lmgtfy");
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREFS_BUDDIES,
					"List of buddies (separated by comma) who will receive automatic replies or 'all' to send autoreplies to everybody.");
	purple_plugin_pref_frame_add(frame, pref);

	return frame;
}

static PurplePluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

/*end prefs*/

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,		/* Magic				*/
	PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,		/* plugin type			*/
	NULL,						/* ui requirement		*/
	0,							/* flags				*/
	NULL,						/* dependencies			*/
	PURPLE_PRIORITY_DEFAULT,	/* priority				*/

	PLUGIN_ID,					/* plugin id			*/
	NULL,						/* name					*/
	"1.1.0",					/* version				*/
	NULL,						/* summary				*/
	NULL,						/* description			*/
	PLUGIN_AUTHOR,				/* author				*/
	"http://linuxandwhatever.wordpress.com/2009/09/13/stfw-pidgin-plugin/",
		/* website				*/

	plugin_load,				/* load					*/
	plugin_unload,				/* unload				*/
	NULL,						/* destroy				*/

	NULL,						/* ui_info				*/
	NULL,						/* extra_info			*/
	&prefs_info,				/* prefs_info			*/
	NULL,						/* actions				*/

	NULL,						/* reserved 1			*/
	NULL,						/* reserved 2			*/
	NULL,						/* reserved 3			*/
	NULL						/* reserved 4			*/
};

static void
init_plugin(PurplePlugin *plugin)
{
	info.name = "STFW";
	info.summary = "Lets you send your buddies links to lmgtfy.com";
	info.description = "You can do this automatically or by using the /stfw (or /lmgtfy) command.";
	
	purple_prefs_add_none(PREFS_PREFIX);
	purple_prefs_add_bool(PREFS_AUTO, FALSE);
	purple_prefs_add_string(PREFS_BUDDIES, "all");	
}

PURPLE_INIT_PLUGIN(PLUGIN_STATIC_NAME, init_plugin, info)
