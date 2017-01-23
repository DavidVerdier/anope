/*
 * Anope IRC Services
 *
 * Copyright (C) 2003-2016 Anope Team <team@anope.org>
 *
 * This file is part of Anope. Anope is free software; you can
 * redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see see <http://www.gnu.org/licenses/>.
 */


#include "module.h"
#include "modules/operserv/ignore.h"

class IgnoreImpl : public Ignore
{
	friend class IgnoreType;

	Serialize::Storage<Anope::string> mask, creator, reason;
	Serialize::Storage<time_t> time;

 public:
	using Ignore::Ignore;

	Anope::string GetMask() override;
	void SetMask(const Anope::string &) override;

	Anope::string GetCreator() override;
	void SetCreator(const Anope::string &) override;

	Anope::string GetReason() override;
	void SetReason(const Anope::string &) override;

	time_t GetTime() override;
	void SetTime(const time_t &) override;
};

class IgnoreType : public Serialize::Type<IgnoreImpl>
{
 public:
 	Serialize::Field<IgnoreImpl, Anope::string> mask, creator, reason;
 	Serialize::Field<IgnoreImpl, time_t> time;

	IgnoreType(Module *me) : Serialize::Type<IgnoreImpl>(me)
 		, mask(this, "mask", &IgnoreImpl::mask)
 		, creator(this, "creator", &IgnoreImpl::creator)
 		, reason(this, "reason", &IgnoreImpl::reason)
 		, time(this, "time", &IgnoreImpl::time)
 	{
 	}
};

Anope::string IgnoreImpl::GetMask()
{
	return Get(&IgnoreType::mask);
}

void IgnoreImpl::SetMask(const Anope::string &m)
{
	Set(&IgnoreType::mask, m);
}

Anope::string IgnoreImpl::GetCreator()
{
	return Get(&IgnoreType::creator);
}

void IgnoreImpl::SetCreator(const Anope::string &c)
{
	Set(&IgnoreType::creator, c);
}

Anope::string IgnoreImpl::GetReason()
{
	return Get(&IgnoreType::reason);
}

void IgnoreImpl::SetReason(const Anope::string &r)
{
	Set(&IgnoreType::reason, r);
}

time_t IgnoreImpl::GetTime()
{
	return Get(&IgnoreType::time);
}

void IgnoreImpl::SetTime(const time_t &t)
{
	Set(&IgnoreType::time, t);
}

class OSIgnoreService : public IgnoreService
{
 public:
	OSIgnoreService(Module *o) : IgnoreService(o)
	{
	}

	Ignore *Find(const Anope::string &mask) override
	{
		User *u = User::Find(mask, true);
		std::vector<Ignore *> ignores = Serialize::GetObjects<Ignore *>();
		std::vector<Ignore *>::iterator ign = ignores.begin(), ign_end = ignores.end();

		if (u)
		{
			for (; ign != ign_end; ++ign)
			{
				Entry ignore_mask("", (*ign)->GetMask());
				if (ignore_mask.Matches(u, true))
					break;
			}
		}
		else
		{
			size_t user, host;
			Anope::string tmp;
			/* We didn't get a user.. generate a valid mask. */
			if ((host = mask.find('@')) != Anope::string::npos)
			{
				if ((user = mask.find('!')) != Anope::string::npos)
				{
					/* this should never happen */
					if (user > host)
						return NULL;
					tmp = mask;
				}
				else
					/* We have user@host. Add nick wildcard. */
				tmp = "*!" + mask;
			}
			/* We only got a nick.. */
			else
				tmp = mask + "!*@*";

			for (; ign != ign_end; ++ign)
				if (Anope::Match(tmp, (*ign)->GetMask(), false, true))
					break;
		}

		/* Check whether the entry has timed out */
		if (ign != ign_end)
		{
			Ignore *id = *ign;

			if (id->GetTime() && !Anope::NoExpire && id->GetTime() <= Anope::CurTime)
			{
				this->GetOwner()->logger.Bot("OperServ").Category("expire/ignore").Log(_("Expiring ignore for {0}"), id->GetMask());
				id->Delete();
			}
			else
			{
				return id;
			}
		}

		return NULL;
	}
};

class CommandOSIgnore : public Command
{
	ServiceReference<IgnoreService> ignore_service;
	
 private:
	Anope::string RealMask(const Anope::string &mask)
	{
		/* If it s an existing user, we ignore the hostmask. */
		User *u = User::Find(mask, true);
		if (u)
			return "*!*@" + u->host;

		size_t host = mask.find('@');
		/* Determine whether we get a nick or a mask. */
		if (host != Anope::string::npos)
		{
			size_t user = mask.find('!');
			/* Check whether we have a nick too.. */
			if (user != Anope::string::npos)
			{
				if (user > host)
					/* this should never happen */
					return "";
				else
					return mask;
			}
			else
			{
				/* We have user@host. Add nick wildcard. */
				return "*!" + mask;
			}
		}

		/* We only got a nick.. */
		return mask + "!*@*";
	}

	void DoAdd(CommandSource &source, const std::vector<Anope::string> &params)
	{
		const Anope::string &time = params.size() > 1 ? params[1] : "";
		const Anope::string &nick = params.size() > 2 ? params[2] : "";
		const Anope::string &reason = params.size() > 3 ? params[3] : "";

		if (time.empty() || nick.empty())
		{
			this->OnSyntaxError(source, "ADD");
			return;
		}

		time_t t = Anope::DoTime(time);

		if (t <= -1)
		{
			source.Reply(_("Invalid expiry time \002{0}\002."), time);
			return;
		}

		Anope::string mask = RealMask(nick);
		if (mask.empty())
		{
			source.Reply(_("Mask must be in the form \037user\037@\037host\037."));
			return;
		}

		if (Anope::ReadOnly)
			source.Reply(_("Services are in read-only mode. Any changes made may not persist."));

		Ignore *ign = Serialize::New<Ignore *>();
		ign->SetMask(mask);
		ign->SetCreator(source.GetNick());
		ign->SetReason(reason);
		ign->SetTime(t ? Anope::CurTime + t : 0);

		if (!t)
		{
			source.Reply(_("\002{0}\002 will now permanently be ignored."), mask);

			logger.Command(LogType::ADMIN, source, _("{source} used {command} to add a permanent ignore for {0}"), mask);
		}
		else
		{
			source.Reply(_("\002{0}\002 will now be ignored for \002{1}\002."), mask, Anope::Duration(t, source.GetAccount()));

			logger.Command(LogType::ADMIN, source, _("{source} used {command} to add an ignore on {0} for {1}"), mask, Anope::Duration(t));
		}
	}

	void DoList(CommandSource &source)
	{
		std::vector<Ignore *> ignores = Serialize::GetObjects<Ignore *>();

		for (Ignore *id : ignores)
		{
			if (id->GetTime() && !Anope::NoExpire && id->GetTime() <= Anope::CurTime)
			{
				this->GetOwner()->logger.Bot("OperServ").Category("expire/ignore").Log(_("Expiring ignore entry {0}"), id->GetMask());
				id->Delete();
			}
		}

		ignores = Serialize::GetObjects<Ignore *>();
		if (ignores.empty())
		{
			source.Reply(_("Ignore list is empty."));
			return;
		}

		ListFormatter list(source.GetAccount());
		list.AddColumn(_("Mask")).AddColumn(_("Creator")).AddColumn(_("Reason")).AddColumn(_("Expires"));

		for (Ignore *ignore : ignores)
		{
			ListFormatter::ListEntry entry;
			entry["Mask"] = ignore->GetMask();
			entry["Creator"] = ignore->GetCreator();
			entry["Reason"] = ignore->GetReason();
			entry["Expires"] = Anope::Expires(ignore->GetTime(), source.GetAccount());
			list.AddEntry(entry);
		}

		source.Reply(_("Services ignore list:"));

		std::vector<Anope::string> replies;
		list.Process(replies);

		for (const Anope::string &r : replies)
			source.Reply(r);
	}

	void DoDel(CommandSource &source, const std::vector<Anope::string> &params)
	{
		const Anope::string nick = params.size() > 1 ? params[1] : "";
		if (nick.empty())
		{
			this->OnSyntaxError(source, "DEL");
			return;
		}

		Anope::string mask = RealMask(nick);
		if (mask.empty())
		{
			source.Reply(_("Mask must be in the form \037user\037@\037host\037."));
			return;
		}

		Ignore *ign = ignore_service->Find(mask);
		if (!ign)
		{
			source.Reply(_("\002{0}\002 not found on ignore list."), mask);	
			return;
		}

		if (Anope::ReadOnly)
			source.Reply(_("Services are in read-only mode. Any changes made may not persist."));

		logger.Command(LogType::ADMIN, source, _("{source} used {command} to remove an ignore on {0}"), mask);

		source.Reply(_("\002{0}\002 will no longer be ignored."), mask);
		ign->Delete();
	}

	void DoClear(CommandSource &source)
	{
		if (Anope::ReadOnly)
			source.Reply(_("Services are in read-only mode. Any changes made may not persist."));

		for (Ignore *ign : Serialize::GetObjects<Ignore *>())
			ign->Delete();

		logger.Command(LogType::ADMIN, source, _("{source} used {command} to CLEAR the ignore list"));

		source.Reply(_("Ignore list has been cleared."));
	}

 public:
	CommandOSIgnore(Module *creator) : Command(creator, "operserv/ignore", 1, 4)
	{
		this->SetDesc(_("Modify the Services ignore list"));
		this->SetSyntax(_("ADD \037expiry\037 {\037nick\037|\037mask\037} [\037reason\037]"));
		this->SetSyntax(_("DEL {\037nick\037|\037mask\037}"));
		this->SetSyntax("LIST");
		this->SetSyntax("CLEAR");
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) override
	{
		const Anope::string &cmd = params[0];

		if (cmd.equals_ci("ADD"))
			return this->DoAdd(source, params);
		else if (cmd.equals_ci("LIST"))
			return this->DoList(source);
		else if (cmd.equals_ci("DEL"))
			return this->DoDel(source, params);
		else if (cmd.equals_ci("CLEAR"))
			return this->DoClear(source);
		else
			this->OnSyntaxError(source, "");
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand) override
	{
		source.Reply(_("The \002{0}\002 command allows you to make services ignore a user or hostmask."
		               " \037expiry\037 must be an integer and can be followed by one of \037d\037 (days), \037h\037 (hours), or \037m\037 (minutes)."
		               " If a unit specifier is not included, the default is seconds."
		               " To make services permanently ignore the user, use 0 as the expiry time."
		               " When adding a \037mask\037, it should be in the format nick!user@host, everything else will be considered a nicknames. Wildcards are permitted."),
		               source.command);

		const Anope::string &regexengine = Config->GetBlock("options")->Get<Anope::string>("regexengine");
		if (!regexengine.empty())
		{
			source.Reply(" ");
			source.Reply(_("Regex matches are also supported using the \002{0}\002 engine. Enclose your pattern in // if this is desired."), regexengine);
		}

		return true;
	}
};

class OSIgnore : public Module
	, public EventHook<Event::BotPrivmsg>
{
	IgnoreType ignoretype;
	OSIgnoreService osignoreservice;
	CommandOSIgnore commandosignore;

 public:
	OSIgnore(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, VENDOR)
		, EventHook<Event::BotPrivmsg>(this)
		, ignoretype(this)
		, osignoreservice(this)
		, commandosignore(this)
	{

	}

	EventReturn OnBotPrivmsg(User *u, ServiceBot *bi, Anope::string &message) override
	{
		if (!u->HasMode("OPER") && this->osignoreservice.Find(u->nick))
			return EVENT_STOP;

		return EVENT_CONTINUE;
	}
};

MODULE_INIT(OSIgnore)