# mod-level-rewards

Un sistema de recompensas modular y basado en bases de datos para AzerothCore. Este módulo recompensa a los jugadores al alcanzar hitos de nivel específicos con una combinación de oro automatizado, hechizos y "Cofres de Recompensa" personalizados que ofrecen botín ponderado por niveles.

## Características
* **Sistema de Recompensas por Niveles:** Otorga automáticamente diferentes niveles de Cofres de Recompensa (Blanco, Verde, Azul, Morado) basados en rangos de nivel.
* **Generación Inteligente de Botín:** El botín de los cofres se genera dinámicamente según la clase del jugador, la competencia con armaduras y el rango de nivel.
* **Pools de Jackpot Ponderados:** Incluye una probabilidad de 1/1000 para objetos legendarios/de nivel TCG, totalmente configurables.
* **Soporte de Localización:** Soporte fácilmente extensible para múltiples idiomas, tanto en anuncios globales como en notificaciones personales.
* **Gestión de Inventario:** Sistema de respaldo por correo integrado para cuando las bolsas del jugador están llenas.
* **Protección contra Bots:** Filtrado automático para ignorar cuentas tipo RNDBOT.

## Instalación
1. Coloca la carpeta `mod-level-rewards` en tu directorio `modules`.
2. Ejecuta `cmake` para configurar los archivos de tu servidor.
3. Aplica los archivos SQL (ubicados en `sql/`) a tu base de datos `acore_world`.
4. Recompila tu servidor.

## Estructura de la Base de Datos

### `mod_level_rewards_items`
Esta tabla define las recompensas estáticas (oro/hechizos) otorgadas en hitos específicos.

| Columna | Tipo | Descripción |
| :--- | :--- | :--- |
| `level` | `tinyint` | El nivel en el que se activa la recompensa. |
| `money` | `int` | Oro adicional (multiplicado por la constante GOLD). |
| `spell` | `int` | ID del hechizo a lanzar o aprender. |
| `learn` | `boolean` | Si es verdadero, el jugador aprende el hechizo; si es falso, se lanza el hechizo. |
| `itemId1` | `int` | ID del objeto de recompensa principal opcional. |
| `itemId2` | `int` | ID del objeto de recompensa secundario opcional. |
| `race`/`class`| `tinyint` | Filtro por raza/clase (0 para todas). |

## Configuración
Puedes configurar el comportamiento del módulo a través de tu archivo `worldserver.conf`:

* `Congrats.Enable` (predeterminado: 1) - Activa/desactiva el sistema del módulo.
* `Congrats.Announce` (predeterminado: 1) - Activa/desactiva el anuncio global de inicio.
* `CongratsPerLevel.Enable` (predeterminado: 1) - Activa/desactiva el anuncio de "¡DING!" al subir de nivel.
* `Congrats.Rewards.Enable` (predeterminado: 1) - Activa/desactiva la distribución de Cofres de Recompensa.
* `Congrats.Acore.String.ID` (predeterminado: 60000) - El ID de acore_string para el anuncio de inicio.

## Personalización
* **Tablas de Botín:** Para modificar las tasas de obtención o el contenido de los Cofres de Recompensa, edita `src/mod_level_rewards.cpp` y actualiza los vectores `WeightedItem` en `GetJackpotPool` y `GetPool`.
* **Añadir Objetos:** Asegúrate de que cualquier mapeo personalizado de `DisplayInfoID` se aplique a tu `Item.dbc` si añades nuevos modelos de objetos.

## Licencia
Este módulo se publica bajo la licencia GNU AGPL v3, de manera coherente con el proyecto AzerothCore.