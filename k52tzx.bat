rem k52wav "Bomber (1986) (Hebdogiciel) (PD).k5"
rem sox "Bomber (1986) (Hebdogiciel) (PD).wav" "Bomber (1986) (Hebdogiciel) (PD).voc"
rem direct "Bomber (1986) (Hebdogiciel) (PD).voc"
rem direct /t 1464 "Bomber (1986) (Hebdogiciel) (PD).voc" "Bomber (1986) (Hebdogiciel) (PD)_2400Hz.tzx"
rem PAUSE

ren *.k7 *.k5
forfiles /m *.k5 /C "cmd /c k52wav @file
forfiles /m *.wav /C "cmd /c sox @file @fname.voc
del *.wav
forfiles /m *.voc /C "cmd /c direct  /t 1464 @file @fname_2400Hz.tzx
del *.voc
mkdir K5TZX
move *.tzx K5TZX
PAUSE
