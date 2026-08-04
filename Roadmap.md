# Arterium
## Core Gameplay

- limited traversable terrain
- abundant but scattered resources
- density-preseving transportation
- factory production line
- AI trading, market needs
- AI employment, education
- multiplayer

## Implementation Checklist

- [*] Server-Client Protocol
    - [*] ClientScene -> wraps SceneExt
    - [*] ServerScene -> is Scene
    - [*] StandaloneScene -> wraps Scene and SceneExt
    - [*] NetObject synchronization (Component, owned by entity)
    - [*] NetEventStream synchronization (Latest & In order)
    - [*] Chunk streaming
    - [*] entity create/destroy
    - [*] component update
