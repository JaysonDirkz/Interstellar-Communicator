/*
Versie:
Poly en mono gescheiden.
Eerst channel diffrentieren in cycling, trigger, split.

Logboek:
Interstellar_Communicator_PWM_v28 [30-5-22]:
- Gemiddelde calculatie voor ATP erin gezet met ondersteunende code.
Interstellar_Communicator_PWM_v28 [7-5-22]:
- Polyphonic aftertouch werkend gemaakt.
- Pitchbend toegevoed in keylearn.
- Keystypes inleren prioriteit gegeven.
Interstellar_Communicator_PWM_v26 [4-5-22]:
- Bijna alles erin en getest.
Interstellar_Communicator_PWM_v24 [3-4-22]:
- Alle logic omgebouwd.
Interstellar_Communicator_PWM_v22 [22-2-22]:
- Gedeeltelijk Debugged.
Interstellar_Communicator_PWM_v21 [16-1-22]:
- Aangepast voor unipolaire CV.
Interstellar_Communicator_PWM_v20 [3-6-21]:
- Logboek aangemaakt.
- Midi clock ticks veranderd naar altijd output.

BUGS:
  ...

TESTEN:
- Werking KeyAftertouch gemiddelde van ingedrukte toetsen per rij.
- Channelaftertouch inleren als er geen polyaftertouch is.
- Prioriteit van polyaftertouch over channelaftertouch.
- ChannelAftertouch werking.
- ControlChange.
- Percussie.
- Keys split.


- Als alle functies erin zitten doe dan de laatste test:
- Test op glitches wanneer je veel midi ontvangt met soft midi thru aan.

    ( Ontwikkel een gestandaardiseerde test in Max waarbij je de snelheid van midi berichten kan regelen ).
    ( Ontwikkel ook een test op je Korg ESX-1 ).
    ( Ontwikkel ook een test op een Arduino Nano ).

    Bepaal het snelheidspunt waarop berichten niet meer doorkomen.
    Test bijvoorbeeld of note offs niet geregistreerd worden. (Doe dan tijdelijk de note off logic van Active Sensing uit).
    
    - Test dan opnieuw met soft midi thru uit:
        Worden het minder erg?: Kijk of je midithru in hardware kan door-routen.
        Niet minder? Heeft geen effect dus geen hardware route.


TODO:
- Voeg ook 3 percussie kanalen toe met choke groups.
    Bij een choke wordt de velocity wordt laaggebracht.
    1 kanaal met duo choke:
        Alleen de bovenste twee outputs en de onderste twee outputs kappen elkaars velocity af.
    2 kanalen met total choke:
        Alle outputs binnen 1 channel kappen elkaars velocity af.

     suggestie:
        midi kanaal 9 = geen choke.
        midi kanaal 10 = duo choke.
        midi kanaal 11 = total choke.
        midi kanaal 12 = total choke.

- ControlChange nog loop weghalen als het kan.
- Data besparen?:
    Verminderen aantal midi kanalen van learn. (nu 16).

- Suggestie, eerst huidige werking testen: Met polyphonics keys en monophonic velocity: enkel van de eerste noot velocity schrijven.

- Als alle learn/speel functies getest zijn: Active sensing timeout weer inbrengen.

- Dubbelklik wisfunctie.
- EEPROM.
*/
