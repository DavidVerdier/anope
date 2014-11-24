/*
 *
 * Copyright (C) 2008-2011 Robin Burchell <w00t@inspircd.org>
 * Copyright (C) 2008-2014 Anope Team <team@anope.org>
 *
 * Please read COPYING and README for further details.
 *
 */

#include "services.h"
#include "commands.h"
#include "users.h"
#include "language.h"
#include "config.h"
#include "opertype.h"
#include "channels.h"
#include "event.h"
#include "bots.h"
#include "protocol.h"
#include "modules/botserv.h"
#include "modules/chanserv.h"

CommandSource::CommandSource(const Anope::string &n, User *user, NickServ::Account *core, CommandReply *r, ServiceBot *bi) : nick(n), u(user), nc(core), reply(r),
	c(NULL), service(bi)
{
}

const Anope::string &CommandSource::GetNick() const
{
	return this->nick;
}

User *CommandSource::GetUser()
{
	return this->u;
}

NickServ::Account *CommandSource::GetAccount()
{
	return this->nc;
}

ChanServ::AccessGroup CommandSource::AccessFor(ChanServ::Channel *ci)
{
	if (this->u)
		return ci->AccessFor(this->u);
	else if (this->nc)
		return ci->AccessFor(this->nc);
	else
		return ChanServ::AccessGroup();
}

bool CommandSource::IsFounder(ChanServ::Channel *ci)
{
	if (this->u)
		return ci->IsFounder(this->u);
	else if (this->nc)
		return *this->nc == ci->GetFounder();
	return false;
}

bool CommandSource::HasCommand(const Anope::string &cmd)
{
	if (this->u)
		return this->u->HasCommand(cmd);
	else if (this->nc && this->nc->o)
		return this->nc->o->GetType()->HasCommand(cmd);
	return false;
}

bool CommandSource::HasPriv(const Anope::string &cmd)
{
	if (this->u)
		return this->u->HasPriv(cmd);
	else if (this->nc && this->nc->o)
		return this->nc->o->GetType()->HasPriv(cmd);
	return false;
}

bool CommandSource::IsServicesOper()
{
	if (this->u)
		return this->u->IsServicesOper();
	else if (this->nc)
		return this->nc->IsServicesOper();
	return false;
}

bool CommandSource::IsOper()
{
	if (this->u)
		return this->u->HasMode("OPER");
	else if (this->nc)
		return this->nc->IsServicesOper();
	return false;
}

void CommandSource::Reply(const Anope::string &message)
{
	const char *translated_message = Language::Translate(this->nc, message.c_str());

	sepstream sep(translated_message, '\n', true);
	Anope::string tok;
	while (sep.GetToken(tok))
		this->reply->SendMessage(*this->service, tok);
}

Command::Command(Module *o, const Anope::string &sname, size_t minparams, size_t maxparams) : Service(o, "Command", sname), max_params(maxparams), min_params(minparams), module(owner)
{
	allow_unregistered = require_user = false;
}

Command::~Command()
{
}

void Command::SetDesc(const Anope::string &d)
{
	this->desc = d;
}

void Command::ClearSyntax()
{
	this->syntax.clear();
}

void Command::SetSyntax(const Anope::string &s)
{
	this->syntax.push_back(s);
}

void Command::SendSyntax(CommandSource &source)
{
	Anope::string s = Language::Translate(source.GetAccount(), _("Syntax"));
	if (!this->syntax.empty())
	{
		source.Reply("{0}: \002{1} {2}\002", s, source.command, Language::Translate(source.GetAccount(), this->syntax[0].c_str()));
		Anope::string spaces(s.length(), ' ');
		for (unsigned i = 1, j = this->syntax.size(); i < j; ++i)
			source.Reply("{0}  \002{1} {2}\002", spaces, source.command, Language::Translate(source.GetAccount(), this->syntax[i].c_str()));
	}
	else
		source.Reply("{0}: \002{1}\002", s, source.command);
}

bool Command::AllowUnregistered() const
{
	return this->allow_unregistered;
}

void Command::AllowUnregistered(bool b)
{
	this->allow_unregistered = b;
}

bool Command::RequireUser() const
{
	return this->require_user;
}

void Command::RequireUser(bool b)
{
	this->require_user = b;
}

const Anope::string Command::GetDesc(CommandSource &) const
{
	return this->desc;
}

void Command::OnServHelp(CommandSource &source)
{
	source.Reply(Anope::printf("    %-14s %s", source.command.c_str(), Language::Translate(source.nc, this->GetDesc(source).c_str())));
}

bool Command::OnHelp(CommandSource &source, const Anope::string &subcommand) { return false; }

void Command::OnSyntaxError(CommandSource &source, const Anope::string &subcommand)
{
	this->SendSyntax(source);
	bool has_help = source.service->commands.find("HELP") != source.service->commands.end();
	if (has_help)
		source.Reply(_("\002{0}{1} HELP {2}\002 for more information."), Config->StrictPrivmsg, source.service->nick, source.command);
}

void Command::Run(CommandSource &source, const Anope::string &message)
{
	std::vector<Anope::string> params;
	spacesepstream(message).GetTokens(params);
	bool has_help = source.service->commands.find("HELP") != source.service->commands.end();

	CommandInfo::map::const_iterator it = source.service->commands.end();
	unsigned count = 0;
	for (unsigned max = params.size(); it == source.service->commands.end() && max > 0; --max)
	{
		Anope::string full_command;
		for (unsigned i = 0; i < max; ++i)
			full_command += " " + params[i];
		full_command.erase(full_command.begin());

		++count;
		it = source.service->commands.find(full_command);
	}

	if (it == source.service->commands.end())
	{
		if (has_help)
			source.Reply(_("Unknown command \002%s\002. \"%s%s HELP\" for help."), message.c_str(), Config->StrictPrivmsg.c_str(), source.service->nick.c_str());
		else
			source.Reply(_("Unknown command \002%s\002."), message.c_str());
		return;
	}

	const CommandInfo &info = it->second;
	ServiceReference<Command> c("Command", info.name);
	if (!c)
	{
		if (has_help)
			source.Reply(_("Unknown command \002%s\002. \"%s%s HELP\" for help."), message.c_str(), Config->StrictPrivmsg.c_str(), source.service->nick.c_str());
		else
			source.Reply(_("Unknown command \002%s\002."), message.c_str());
		Log(source.service) << "Command " << it->first << " exists on me, but its service " << info.name << " was not found!";
		return;
	}

	if (c->RequireUser() && !source.GetUser())
		return;

	// Command requires registered users only
	if (!c->AllowUnregistered() && !source.nc)
	{
		source.Reply(_("Password authentication required for that command."));
		if (source.GetUser())
			Log(LOG_NORMAL, "access_denied_unreg", source.service) << "Access denied for unregistered user " << source.GetUser()->GetMask() << " with command " << it->first;
		return;
	}

	for (unsigned i = 0, j = params.size() - (count - 1); i < j; ++i)
		params.erase(params.begin());

	while (c->max_params > 0 && params.size() > c->max_params)
	{
		params[c->max_params - 1] += " " + params[c->max_params];
		params.erase(params.begin() + c->max_params);
	}

	source.command = it->first;
	source.permission = info.permission;

	EventReturn MOD_RESULT;
	MOD_RESULT = Event::OnPreCommand(&Event::PreCommand::OnPreCommand, source, c, params);
	if (MOD_RESULT == EVENT_STOP)
		return;

	if (params.size() < c->min_params)
	{
		c->OnSyntaxError(source, !params.empty() ? params[params.size() - 1] : "");
		return;
	}

	// If the command requires a permission, and they aren't registered or don't have the required perm, DENIED
	if (!info.permission.empty() && !source.HasCommand(info.permission))
	{
		if (!source.IsOper())
			source.Reply(_("Access denied. You are not a Services Operator."));
		else
			source.Reply(_("Access denied. You do not have access to command \002{0}\002."), info.permission);
		if (source.GetUser())
			Log(LOG_NORMAL, "access_denied", source.service) << "Access denied for user " << source.GetUser()->GetMask() << " with command " << it->first;
		return;
	}

	c->Execute(source, params);
	Event::OnPostCommand(&Event::PostCommand::OnPostCommand, source, c, params);
}

bool Command::FindCommandFromService(const Anope::string &command_service, ServiceBot* &bot, Anope::string &name)
{
	bot = NULL;

	for (std::pair<Anope::string, User *> p : UserListByNick)
	{
		User *u = p.second;
		if (u->type != UserType::BOT)
			continue;

		ServiceBot *bi = anope_dynamic_static_cast<ServiceBot *>(u);

		for (CommandInfo::map::const_iterator cit = bi->commands.begin(), cit_end = bi->commands.end(); cit != cit_end; ++cit)
		{
			const Anope::string &c_name = cit->first;
			const CommandInfo &info = cit->second;

			if (info.name != command_service)
				continue;

			bot = bi;
			name = c_name;
			return true;
		}
	}

	return false;
}

