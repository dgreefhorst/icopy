	;; Call CIO

	.export _ciov, _dciov
	.import _yvar
	
_ciov:	LDX #$00
	JSR $E456
	RTS
	
_dciov:	ASL
	ASL
	ASL
	ASL
	TAX
	JSR $E456
	STY _yvar
	RTS
