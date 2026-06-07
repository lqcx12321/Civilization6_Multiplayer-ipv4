package utils

import (
	"net"
	"regexp"
)

func IsValidIPv4(ip string) bool {
	ipv4 := net.ParseIP(ip)
	if ipv4 == nil {
		return false
	}
	return ipv4.To4() != nil
}

func IsValidDomainRegex(domain string) bool {
	regex := `^(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}$`
	match, _ := regexp.MatchString(regex, domain)
	return match
}

func IsValidDomain(domain string) bool {
	if _, err := net.LookupHost(domain); err == nil {
		return true
	}
	return false
}
