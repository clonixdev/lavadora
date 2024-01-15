# lavadora.ino
Basado en el código de santiieuse11/lavadora.ino


Lavadora programada con arduino para cuando se  daña el control original, timer o programador.

En mi proyecto use arduino UNO , 7 reles optoacoplados , buzzer pasivo y filtros RC (snubber)

El motor es de 5 pines lo que use 4 reles para controlarlo:

- 1x Encendido y apagado del motor (este tiene que tener un filtro RC para filtrar las cargas inductivas del motor)
- 1x Control del sentido de giro
- 2x reles para conmutar los pines restantes del motor

En esquema_motor.jpg se ven 3 reles , hay que agregar 1 más entre el giro y la fase para el encendido y apagado.

Luego use 3 reles más para controlar:
- 1x valvula de entrada *FILTRO RC
- 1x bomba de desagote *FILTRO RC
- 1x bloqueo de puerta

!Importante los reles del motor, valvula y bomba tienen que tener filtros snubber para evitar las cargas inductivas , si no se utilizan se van a producir interferencias haciendo que se pare o reinicie el arduino o que los reles se dañen.
Para lograr una aislación completa deben usarse dos fuentes de alimentación: 1 para los reles y otra para arduino (reles_conexion)

Luego de montar todo decidi quitar la pantalla para no cortar el frente de la lavadora y no tener filtraciones de agua en caso de derrames.
Al finalizar reproduce una cancion en el buzzer.