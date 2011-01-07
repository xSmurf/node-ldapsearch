#!/usr/bin/env node

var sys		= require("sys"),
LDAPClient	= require("./build/default/ldap.node"); // Path to ldapauth.node

LDAPClient.Search("ldaps://ldap.example.tld/ou=people,dc=example,dc=tld?*?sub?uid=*",
	function(err, result) {
		if (err) {
			sys.puts(err);
		} else {
			sys.puts("Result: ");
			console.log(result);
		}
	}
);