# Distributed Cloud Server Architecture Design — Kung-Fu Chess

## Overview
This document outlines the system design for scaling the **Kung-Fu Chess** server infrastructure to support **100 million registered users** and **10 million concurrent active players** worldwide.

---

## 1. Database Architecture: 100 Million Registered Users

### Is SQLite Suitable?
**No, SQLite is NOT suitable for this scale.**

#### Why SQLite Fails at 100M Users:
1. **Single-File File-Locking Concurrency**: SQLite locks the entire database file on write operations (`WAL` mode improves readers, but still serializes concurrent writers). With 10M concurrent users generating authentication events, rating updates, and match results, thousands of write operations happen per second. SQLite will bottleneck instantly with `SQLITE_BUSY` lock contention.
2. **Horizontal Scaling & Distributed Replication**: SQLite lives on a single disk attached to a single process. It cannot be partitioned (sharded) natively across multiple nodes.
3. **No Native Network Protocol**: SQLite is an embedded library, not a networked database server. Multiple game server instances running in Docker containers cannot safely write to a shared remote SQLite file without network file locks (NFS), which are notoriously slow and prone to corruption.

### Recommended Database Solution
A hybrid distributed database strategy:

1. **Relational Database for Profiles, Auth & Historical Match Logs**:
   - **Managed PostgreSQL Cluster (AWS Aurora PostgreSQL / GCP Cloud SQL)** sharded by `user_id` or powered by **CockroachDB / Citus**:
     - Handles ACID transactions for registration, bcrypt passwords, ELO ratings, and historical game records.
     - Sharded horizontally; read-replicas distributed across multiple availability zones (AZs) handle high read volume (user profile views, leaderboards).
2. **In-Memory Caching & Session Layer**:
   - **Redis Cluster (In-Memory Key-Value)**:
     - Stores active user session tokens, online state, matchmaking queues, and real-time room-to-pod routing maps (`room_id -> pod_ip:port`).
     - Sub-millisecond read/write latency.
3. **Asynchronous Write Buffer / Message Queue**:
   - **Apache Kafka / AWS SQS / RabbitMQ**:
     - Decouples real-time C++ game engines from persistent database writes. When a match ends, the game engine publishes an event to the queue and immediately returns to processing ticks. Background worker pods consume the queue and write results to PostgreSQL asynchronously.

---

## 2. Global Concurrency & Cloud Architecture Proposal

### Can a Single Server Handle 10M Active Players?
**No.** A standard server CPU (e.g., 64-core) can handle at most 10,000–50,000 active concurrent WebSocket connections before exhausting file descriptors (C10K/C1000K problem) and CPU core tick loops.

### Cloud Services & Component Breakdown

| Cloud Service Role | Proposed Cloud Technology (AWS / GCP) | Responsibilities |
| :--- | :--- | :--- |
| **Global Edge & DDoS Protection** | Cloudflare Enterprise / AWS Shield + Route53 | Anycast routing, SSL/TLS termination, Geo-DNS, DDoS protection |
| **API Gateway & Load Balancer** | AWS ALB / Envoy / Nginx Ingress Controller | WebSocket routing, rate limiting, sticky session routing via Redis lookups |
| **Auth & REST Microservices** | AWS EKS / GCP GKE (Stateless Docker Pods) | User login, registration, profile management, leaderboards |
| **Matchmaker Service** | Dedicated EKS Pods + Redis Sorted Sets | ELO-based matchmaking queue, room creation, pod allocation |
| **Game Server Clusters** | AWS EKS / GCP GKE (Stateful C++ Containers) | Runs C++ `GameEngine` / `RealTimeArbiter` 16ms tick loops (~200–500 rooms per container) |
| **Real-Time State & Routing** | AWS ElastiCache for Redis Cluster | Active room registry, online user presence, session tokens |
| **Persistent Storage** | AWS Aurora PostgreSQL (Multi-AZ Sharded) | User accounts, ELO history, game logs |
| **Async Event Bus** | AWS SQS / Apache Kafka | Decouples engine tick loops from DB writes for ELO & match archives |
| **Object & Cold Storage** | AWS S3 / S3 Glacier | Game replays (PGNs), server logs, automated DB snapshot archives |

### Distributed System Topology Diagram

```
                               ┌──────────────────────────────────┐
                               │  Global Anycast / Cloudflare     │
                               └────────────────┬─────────────────┘
                                                │
                               ┌────────────────▼─────────────────┐
                               │  API Gateway / Load Balancers   │
                               │      (Envoy / Nginx / ALB)       │
                               └────────────────┬─────────────────┘
                                                │
                    ┌──────────────────────────┼──────────────────────────┐
                    │                          │                          │
      ┌─────────────▼────────────┐┌────────────▼────────────┐┌─────────────▼────────────┐
      │  Matchmaker Pods (x50)   ││  State Router / Gateway ││  Auth / Profile Pods     │
      │  (Redis Sorted Sets Queue)││  (WebSocket Proxy Router)││  (Stateless REST / gRPC) │
      └─────────────┬────────────┘└────────────┬────────────┘└─────────────┬────────────┘
                    │                          │                          │
                    └──────────────────────────┼──────────────────────────┘
                                               │
                              ┌────────────────▼─────────────────┐
                              │  Redis Cluster & Pub/Sub Router  │
                              │  (Room-to-Pod Mapping & Presence)│
                              └────────────────┬─────────────────┘
                                               │
             ┌─────────────────────────────────┼─────────────────────────────────┐
             │                                 │                                 │
┌────────────▼─────────────┐      ┌────────────▼─────────────┐      ┌────────────▼─────────────┐
│ Game Server Pod Cluster 1│      │ Game Server Pod Cluster 2│      │ Game Server Pod Cluster N│
│ (Handles Rooms 1..200)   │      │ (Handles Rooms 201..400) │      │ (Handles Rooms N..N+200) │
└────────────┬─────────────┘      └────────────┬─────────────┘      └────────────┬─────────────┘
             │                                 │                                 │
             └─────────────────────────────────┼─────────────────────────────────┘
                                               │
                               ┌───────────────▼─────────────────┐
                               │    Kafka / SQS Event Queue      │
                               └───────────────┬─────────────────┘
                                               │
                               ┌───────────────▼─────────────────┐
                               │ PostgreSQL Multi-AZ Cluster     │
                               └─────────────────────────────────┘
```

---

## 3. Communication Protocols & Inter-Service Flow

1. **Client to Gateway (External)**:
   - **WSS (WebSocket Secure)** for real-time game inputs (`CLICK`, move events, time sync).
   - **HTTPS / REST / gRPC** for authentication, user profiles, and leaderboards.
2. **Gateway to Game Pod (Internal)**:
   - When a client connects with a `room_id`, the Gateway queries Redis (`GET room:1234 -> pod_17:3000`).
   - The Gateway proxies the WSS traffic directly to the target Game Server Pod over high-speed internal VPC network.
3. **Game Engine to DB (Asynchronous)**:
   - C++ Game Engine never makes direct synchronous network calls to PostgreSQL during a match.
   - Upon game completion, the pod writes a single message to Kafka/SQS: `{ match_id, winner_id, loser_id, new_elo_w, new_elo_l, pgn_data }`.
   - Async DB Worker Pods consume the queue and update PostgreSQL in batches, shielding the 16ms game tick loop from database latency spikes.

---

## 4. Fault Tolerance, Resilience & Disaster Recovery

### What Happens If a Game Server Pod Fails Mid-Match?
1. **Detection**: Kubernetes Liveness/Readiness probes detect pod death within 2–5 seconds. Redis presence keys expire via TTL.
2. **Impact Management for Short Games (30–90 seconds)**:
   - **Option A (State Recovery via Redis)**: Game server pods push room state snapshots to Redis every 1 second. If Pod A crashes, the Gateway routes active clients to Pod B, which inflates the game state from Redis and resumes seamlessly.
   - **Option B (Graceful Nullification)**: If snapshot recovery introduces latency unacceptable for real-time chess, the system detects pod failure, cancels the active match, refunds rating points (no ELO penalty), and prompts both players to rejoin matchmaking.
3. **Pod Replacement**: Kubernetes Auto-scaler automatically spins up a fresh container replacement instantly.

### What Happens If the Primary Database Fails?
1. **Multi-AZ Automated Failover**:
   - Managed PostgreSQL (AWS Aurora / GCP Cloud SQL) runs in Multi-AZ mode with synchronous standby replication.
   - If the primary DB node goes down, the cloud cloud manager automatically promotes the Standby Replica to Primary in under 30 seconds.
2. **Write Queue Buffering**:
   - Because match results are published to Kafka / SQS first, game servers continue running unaffected during a 30-second DB failover. Messages accumulate safely in SQS/Kafka and are processed once the DB completes failover—**zero lost match data**.
3. **Read Replicas**:
   - Profile views and leaderboards run against Read Replicas, ensuring user browsing remains 100% operational even if write operations are briefly paused.

### Backup Strategy & Disaster Recovery
1. **Continuous Point-In-Time Recovery (PITR)**:
   - PostgreSQL Write-Ahead Logs (WAL) are streamed continuously to S3/Cloud Storage, allowing restoration to any exact second within the last 35 days.
2. **Automated Daily Snapshots**:
   - Full database snapshots taken nightly and stored across geographically separated regions (Cross-Region Replication).
3. **Cold Storage for Replays**:
   - Historical game PGN logs are stored in low-cost AWS S3 Glacier / GCP Coldline storage.

### What to Do When Disk Space Runs Out?
1. **Prevention via Ephemeral Stateless Containers**:
   - Game server Docker containers write ZERO log files or state to local disk. Standard output (`stdout`/`stderr`) is collected by fluentd/promtail daemonsets and shipped asynchronously to AWS CloudWatch / Datadog / Grafana Loki.
2. **Auto-Expanding Cloud Database Storage**:
   - PostgreSQL runs on auto-scaling storage (e.g., AWS Aurora automatically grows storage up to 128TB as needed without downtime).
3. **Automated Retention & Archiving Policies**:
   - DB tables store only active user records and recent match summaries. Detailed move-by-move histories older than 30 days are archived to S3 and purged from relational DB tables.

---

## 5. Network Traffic & Bandwidth Calculation

### Assumptions:
- **Active Players**: 10,000,000 (5,000,000 active matches of 2 players).
- **Move Frequency**: 1 move every 2 seconds per player = 0.5 moves/sec per player.
- **Total Moves Generated Worldwide**:
  $$\text{Total Moves/sec} = 10,000,000 \text{ players} \times 0.5 \text{ moves/sec} = 5,000,000 \text{ moves/sec}$$

### Message Payload Size:
- **Upstream Move Click Frame**: `CLICK row col` (~20 bytes).
- **Downstream Broadcast State Frame**: `STATE <board_fen_or_binary>` (~120 bytes).
- **Total Data Per Move (Round-Trip + Broadcast to opponent & spectators)**: ~300 bytes.

### Bandwidth Required:
$$\text{Data Rate} = 5,000,000 \text{ moves/sec} \times 300 \text{ bytes/move} = 1,500,000,000 \text{ bytes/sec} \approx 1.5 \text{ GB/sec}$$

Converting to Network Bits:
$$\text{Network Bandwidth} = 1.5 \text{ GB/sec} \times 8 = 12 \text{ Gbps (Gigabits per second)}$$

### Capacity & Throughput Justification:
- **Single Server limit**: 12 Gbps is impossible on one machine.
- **Distributed Cloud Infrastructure**: Spread across 100 Kubernetes ingress load balancers worldwide, each node handles only ~120 Mbps—well within standard cloud network interfaces.

---

## 6. Short Match Duration (30-90 Seconds): Lifecycle of Docker Containers

### What Short Games Mean for Docker Architecture:
1. **Stateless vs. Stateful Lifecycle**:
   - Because matches are brief (30–90 seconds), **Game Server Docker Pods do NOT need long-term persistent disk storage**.
   - Game state lives purely in RAM during the match. Upon match completion, the final result is asynchronously flushed to Kafka/PostgreSQL, and room memory is freed.
2. **Auto-Scaling (K8s Horizontal Pod Autoscaler - HPA)**:
   - High churn (rapid creation and destruction of game rooms) allows server pods to scale up during peak hours and drain smoothly off-peak.
3. **Graceful Pod Termination & Draining**:
   - When deploying updates, a Docker pod stops accepting new matches from the Matchmaker, waits ~90 seconds for active games to finish, and shuts down cleanly with zero dropped games.

---

## Technical Summary for Reviewers (Hebrew Summary)

1. **ארכיטקטורת ענן ושירותים מומלצים**:
   - **PostgreSQL Multi-AZ (עם Sharding)**: מסד נתונים ראשי למשתמשים ודירוגי ELO.
   - **Redis Cluster**: ניהול תור Matchmaking, Session Tokens, ומיפוי מיקום חדרים לשרת המארח (`room_id -> pod_ip`).
   - **Kafka / AWS SQS Queue**: תור הודעות אסינכרוני שמפריד בין מנוע ה-C++ למסד הנתונים, למניעת הנגעת לולאת ה-Tick של השרת בנפילות Latency של ה-DB.
   - **Kubernetes (EKS / GKE)**: אופטימיזציה אלסטית של פודים (Containers) של מנוע המשחק.

2. **התמודדות עם תקלות (Fault Tolerance)**:
   - **קריסת שרת משחק (Game Pod Failure)**: זיהוי תוך 2-5 שניות ע"י K8s. שמירת Snapshots ב-Redis או ביטול משחק הוגן ללא פגיעה בדירוג ה-ELO.
   - **קריסת מסד הנתונים (DB Failure)**: failover אוטומטי ל-Replica תוך פחות מ-30 שניות. תור ה-SQS/Kafka סופג את תוצאות המשחקים בזמן החילוף, כך שאף תוצאת משחק לא אובדת!

3. **גיבויים וניהול דיסק (Disk & Storage Management)**:
   - **דיסק שנגמר**: הקונטיינרים של C++ הם Stateless ואינם כותבים קבצים לדיסק המקומי (Standard Output נשלח ל-CloudWatch/Loki).
   - **גדילת DB**: דיסק אוטונומי המתקפל ומתרחב אוטומטית (AWS Aurora Auto-Scaling up to 128TB).
   - **גיבויים**: Continuous Point-In-Time Recovery (PITR) עד 35 ימים אחורה, וגיבוי יומי ל-S3 Glacier.

