/*
 * Anope IRC Services
 *
 * Copyright (C) 2014-2016 Anope Team <team@anope.org>
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

class NickAccess : public Serialize::Object
{
 public:
	static constexpr const char *const NAME = "nsaccess";

	using Serialize::Object::Object;

	virtual NickServ::Account *GetAccount() anope_abstract;
	virtual void SetAccount(NickServ::Account *) anope_abstract;

	virtual Anope::string GetMask() anope_abstract;
	virtual void SetMask(const Anope::string &) anope_abstract;
};

