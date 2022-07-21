/*
TODO:
- Geen MPE toepassen, want enigste toevoeging hiervan zou duophonic pitchbend zijn.
- Koppel de keysBase aan de noot via eeen noot naar keysBase converter:
    keyBase_from_note[128] = address 0 t/m 3.
- Geld hetzelfde voor midi cc en poly aftertouch.


BUGS:
  ...

TESTEN:
- ActiveKeys2 testen en test ook de wraparound als je 10 toetsten hebt ingedrukt.
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
- Voor poly aftertouch:
        * Gewoon lastNote priority gebruiken (hetzelfde als de noten).
- ALs je data wil besparen, je kan misschien nog historynote en futurenot in een array van 10 zetten.
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
