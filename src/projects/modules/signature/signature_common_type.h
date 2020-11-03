#pragma once

enum class CheckSignatureResult
{
	Error = -2,		// Unexpected error
	Fail = -1,		// Check signature but fail
	Off = 0,		// Signature configuration is off
	Pass = 1		// Check signature and pass
};