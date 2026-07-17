#include "combat/CombatSystem.hpp"

#include "collision/CollisionWorld.hpp"
#include "gameplay/GameplayWorld.hpp"
#include "scene/Scene.hpp"

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>

namespace {
constexpr EntityId playerEntityId = 0xFFFFFFFFFFFFFFFEULL;
constexpr std::size_t maximumProjectiles = 128;
constexpr std::size_t maximumEffects = 512;
constexpr std::size_t maximumDamagePerTick = 256;

bool rayBox(const glm::vec3& origin, const glm::vec3& direction, const AABB& box,
            const float maximumDistance, float& distance, glm::vec3& normal) {
    float nearTime = 0.0F, farTime = maximumDistance; glm::vec3 nearNormal{};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 0.000001F) {
            if (origin[axis] < box.minimum[axis] || origin[axis] > box.maximum[axis]) return false;
            continue;
        }
        float first=(box.minimum[axis]-origin[axis])/direction[axis];
        float second=(box.maximum[axis]-origin[axis])/direction[axis];
        glm::vec3 axisNormal{}; axisNormal[axis]=direction[axis]>0.0F?-1.0F:1.0F;
        if(first>second){std::swap(first,second);axisNormal=-axisNormal;}
        if(first>nearTime){nearTime=first;nearNormal=axisNormal;}
        farTime=std::min(farTime,second); if(nearTime>farTime)return false;
    }
    distance=nearTime; normal=nearNormal; return nearTime<=maximumDistance&&farTime>=0.0F;
}

glm::vec3 translation(const glm::mat4& value) { return glm::vec3{value[3]}; }
} // namespace

bool CombatSystem::initialize(const std::filesystem::path& definitionPath, Scene& scene,
                              GameplayWorld& gameplay, const CollisionWorld& collision,
                              DynamicCollisionWorld& dynamicCollision) {
    std::string error;
    if (!loadCombatDefinitions(definitionPath, definitions_, error)) {
        std::cerr << "Failed to load combat definitions: " << error << '\n'; return false;
    }
    scene_=&scene; gameplay_=&gameplay; collision_=&collision; dynamicCollision_=&dynamicCollision;
    weaponDefinitions_.clear(); enemyDefinitions_.clear();
    for(std::size_t i=0;i<definitions_.weapons.size();++i) weaponDefinitions_[definitions_.weapons[i].id]=i;
    for(std::size_t i=0;i<definitions_.enemies.size();++i) enemyDefinitions_[definitions_.enemies[i].id]=i;
    ammunition_.configure(definitions_.maximumAmmo);
    for (GameplayEntity& entity : gameplay.mutableEntities()) {
        if (entity.authored.type != GameplayEntityType::Pickup ||
            (entity.authored.pickupType != "weapon" && entity.authored.pickupType != "ammo")) {
            continue;
        }
        const bool known = entity.authored.pickupType == "weapon"
            ? weaponDefinition(entity.authored.itemId) != nullptr
            : definitions_.maximumAmmo.contains(entity.authored.itemId);
        if (!known) {
            std::cerr << "Warning: combat pickup '" << entity.authored.name
                      << "' references unknown " << entity.authored.pickupType << " ID '"
                      << entity.authored.itemId << "' and was disabled.\n";
            entity.enabled = false;
            entity.active = false;
            for (const std::size_t index : entity.authored.primitiveIndices) {
                if (index < scene.primitives.size()) scene.primitives[index].visible = false;
            }
        }
    }
    enemies_.clear(); enemiesById_.clear();
    for(GameplayEntity& entity:gameplay.mutableEntities()) {
        if(entity.authored.type!=GameplayEntityType::EnemySpawn) continue;
        const EnemyDefinition* definition=enemyDefinition(entity.authored.enemyType);
        if(definition==nullptr){std::cerr<<"Warning: enemy spawn '"<<entity.authored.name<<"' references unknown type '"<<entity.authored.enemyType<<"'.\n";entity.enabled=false;entity.active=false;for(const std::size_t index:entity.authored.primitiveIndices)if(index<scene.primitives.size())scene.primitives[index].visible=false;continue;}
        EnemyActor actor; actor.id=entity.authored.id; actor.name=entity.authored.name;
        actor.definitionId=entity.authored.enemyType; actor.primitiveIndices=entity.authored.primitiveIndices;
        actor.spawnPosition=translation(entity.authored.authoredWorldTransform) -
            glm::vec3{0.0F, definition->height * 0.5F, 0.0F};
        actor.position=actor.spawnPosition;
        glm::vec3 forward{entity.authored.authoredWorldTransform*glm::vec4{0,0,-1,0}};
        if(glm::length(forward)>0.000001F) actor.forward=glm::normalize(forward);
        actor.health=definition->maximumHealth; actor.startsActive=entity.authored.startsActive;
        actor.active=actor.startsActive; actor.state=EnemyState::Idle;
        enemiesById_[actor.id]=enemies_.size(); enemies_.push_back(std::move(actor));
    }
    reset();
    std::cout<<"Combat summary:\n  weapons:       "<<definitions_.weapons.size()
             <<"\n  ammo types:    "<<definitions_.maximumAmmo.size()
             <<"\n  enemy types:   "<<definitions_.enemies.size()
             <<"\n  enemy actors:  "<<enemies_.size()<<'\n';
    return !definitions_.weapons.empty();
}

void CombatSystem::reset() {
    ammunition_.configure(definitions_.maximumAmmo); weapons_.clear(); equippedIndex_=-1;
    projectiles_.clear(); damageQueue_.clear(); effects_.clear(); rng_.reset(0xC0FFEE1234ULL);
    nextProjectileId_=1; nextDamageSequence_=1; hitMarkerTimer_=killMarkerTimer_=muzzleFlashTimer_=damageIndicatorTimer_=0.0F;
    for(EnemyActor& actor:enemies_){const EnemyDefinition* def=enemyDefinition(actor.definitionId); actor.position=actor.spawnPosition; actor.health=def?def->maximumHealth:1; actor.active=actor.startsActive; actor.state=EnemyState::Idle; actor.stateTimer=actor.attackCooldown=actor.lostSightTimer=0.0F; synchronizeEnemy(actor);}
    if(weaponDefinition("pistol")!=nullptr){static_cast<void>(grantWeapon("pistol",36)); equippedIndex_=0; weapons_[0].state=WeaponState::Ready;}
}

const WeaponDefinition* CombatSystem::weaponDefinition(const std::string_view id) const {auto it=weaponDefinitions_.find(std::string{id});return it==weaponDefinitions_.end()?nullptr:&definitions_.weapons[it->second];}
const EnemyDefinition* CombatSystem::enemyDefinition(const std::string_view id) const {auto it=enemyDefinitions_.find(std::string{id});return it==enemyDefinitions_.end()?nullptr:&definitions_.enemies[it->second];}
EnemyActor* CombatSystem::enemy(const EntityId id) noexcept {auto it=enemiesById_.find(id);return it==enemiesById_.end()?nullptr:&enemies_[it->second];}
const EnemyActor* CombatSystem::enemy(const EntityId id) const noexcept {auto it=enemiesById_.find(id);return it==enemiesById_.end()?nullptr:&enemies_[it->second];}

bool CombatSystem::grantWeapon(const std::string_view id,const int reserveAmmo) {
    const WeaponDefinition* definition=weaponDefinition(id); if(definition==nullptr)return false;
    auto found=std::find_if(weapons_.begin(),weapons_.end(),[id](const WeaponInstance&w){return w.definitionId==id;});
    if(found!=weapons_.end()){if(reserveAmmo>0)static_cast<void>(ammunition_.add(definition->ammoType,reserveAmmo));return false;}
    WeaponInstance instance;instance.definitionId=definition->id;instance.magazine=definition->magazineSize;instance.state=WeaponState::Holstered;
    weapons_.push_back(instance);if(reserveAmmo>0)static_cast<void>(ammunition_.add(definition->ammoType,reserveAmmo));
    if(equippedIndex_<0){equippedIndex_=static_cast<int>(weapons_.size()-1);weapons_.back().state=WeaponState::Ready;} return true;
}
int CombatSystem::addAmmo(const std::string_view type,const int amount){return ammunition_.add(type,amount);}

bool CombatSystem::selectSlot(const int slot) {
    if(slot<0||static_cast<std::size_t>(slot)>=weapons_.size()||slot==equippedIndex_)return false;
    if(equippedIndex_>=0)weapons_[static_cast<std::size_t>(equippedIndex_)].state=WeaponState::Holstered;
    equippedIndex_=slot;WeaponInstance& selected=weapons_[static_cast<std::size_t>(slot)];
    const WeaponDefinition* def=weaponDefinition(selected.definitionId);selected.state=WeaponState::Equipping;selected.timer=def?def->equipTime:0.0F;return true;
}
bool CombatSystem::cycleWeapon(const int direction){if(weapons_.size()<2||direction==0)return false;const int count=static_cast<int>(weapons_.size());return selectSlot((equippedIndex_+(direction>0?1:-1)+count)%count);}

void CombatSystem::fixedUpdate(const float dt,const CombatInput& input,const glm::vec3& cameraPosition,
                               const glm::vec3& cameraForward,const Capsule& playerCapsule,
                               const glm::vec3& playerPosition) {
    if(gameplay_==nullptr)return;
    updateCombatPickups(playerCapsule);
    if(gameplay_->vitals().alive) updateWeapon(dt,input,cameraPosition,cameraForward);
    updateProjectiles(dt,playerPosition); processDamage();
    updateEnemies(dt,playerPosition,playerCapsule); processDamage();
}

void CombatSystem::updateWeapon(const float dt,const CombatInput& input,const glm::vec3& origin,const glm::vec3& forward) {
    if(input.selectSlot>=0)static_cast<void>(selectSlot(input.selectSlot));
    if(input.cycleDirection!=0)static_cast<void>(cycleWeapon(input.cycleDirection));
    if (equippedIndex_ < 0) return;
    WeaponInstance& instance = weapons_[static_cast<std::size_t>(equippedIndex_)];
    const WeaponDefinition* definition=weaponDefinition(instance.definitionId);if(definition==nullptr)return;
    if(instance.timer>0.0F)instance.timer=std::max(0.0F,instance.timer-dt);
    if(instance.state==WeaponState::Equipping&&instance.timer<=0.0F)instance.state=instance.magazine>0?WeaponState::Ready:WeaponState::Empty;
    if((instance.state==WeaponState::Cooldown||instance.state==WeaponState::Firing)&&instance.timer<=0.0F)instance.state=instance.magazine>0?WeaponState::Ready:WeaponState::Empty;
    if(instance.state==WeaponState::Reloading&&instance.timer<=0.0F){const int needed=definition->magazineSize-instance.magazine;const int transfer=std::min(needed,ammunition_.get(definition->ammoType));if(transfer>0&&ammunition_.consume(definition->ammoType,transfer))instance.magazine+=transfer;instance.state=instance.magazine>0?WeaponState::Ready:WeaponState::Empty;}
    if(input.reloadPressed&&(instance.state==WeaponState::Ready||instance.state==WeaponState::Empty)&&instance.magazine<definition->magazineSize&&ammunition_.get(definition->ammoType)>0){instance.state=WeaponState::Reloading;instance.timer=definition->reloadTime;return;}
    const bool fire=definition->automatic?input.fireHeld:input.firePressed;
    if(fire&&(instance.state==WeaponState::Ready||instance.state==WeaponState::Empty)){
        if(instance.magazine<=0){instance.state=WeaponState::Empty;return;}
        --instance.magazine;fireWeapon(instance,*definition,origin,forward);instance.state=WeaponState::Cooldown;instance.timer=1.0F/definition->shotsPerSecond;
    }
}

void CombatSystem::fireWeapon(WeaponInstance& instance,const WeaponDefinition& definition,const glm::vec3& origin,const glm::vec3& forward) {
    ++instance.shotsFired;
    muzzleFlashTimer_ = 0.06F;
    if (definition.attackType == AttackType::Projectile) {
        if (projectiles_.size() >= maximumProjectiles) return;
        Projectile projectile;
        projectile.id = stableEntityId("projectile_" + std::to_string(nextProjectileId_++));
        projectile.owner = playerEntityId;
        projectile.instigator = playerEntityId;
        projectile.position = origin + forward * 0.55F;
        projectile.previousPosition = projectile.position;
        projectile.velocity = rng_.spreadDirection(forward, definition.spreadDegrees) *
                              definition.projectileSpeed;
        projectile.radius = definition.projectileRadius;
        projectile.remainingLifetime = definition.projectileLifetime;
        projectile.directDamage = definition.damage;
        projectile.splashDamage = definition.splashDamage;
        projectile.splashRadius = definition.splashRadius;
        projectile.damageType = DamageType::Projectile;
        projectile.selfDamage = definition.selfDamage;
        projectiles_.push_back(projectile);
        return;
    }

    bool damaged = false;
    bool killed = false;
    lastTrace_ = {};
    for (int pellet = 0; pellet < definition.pellets; ++pellet) {
        const glm::vec3 direction = rng_.spreadDirection(forward, definition.spreadDegrees);
        const HitResult hit = trace(origin, direction, definition.range, playerEntityId);
        lastTrace_ = hit;
        const glm::vec3 end = hit.hit ? hit.point : origin + direction * definition.range;
        if (effects_.size() < maximumEffects) {
            effects_.push_back({origin, end,
                definition.pellets > 1 ? glm::vec3{1.0F, 0.65F, 0.2F}
                                       : glm::vec3{1.0F, 0.9F, 0.35F}, 0.08F});
        }
        if (hit.hit) addImpact(hit);
        if (hit.entity == 0) continue;
        EnemyActor* target = enemy(hit.entity);
        const bool aliveBefore = target != nullptr && target->state != EnemyState::Dead;
        queueDamage({playerEntityId, playerEntityId, hit.entity, definition.damageType,
                     definition.damage, hit.point, hit.normal,
                     direction * definition.damage});
        damaged = true;
        if (aliveBefore && target != nullptr && definition.damage >= target->health) killed = true;
    }
    if (damaged) {
        hitMarkerTimer_ = 0.14F;
        if (killed) killMarkerTimer_ = 0.28F;
    }
}

void CombatSystem::addImpact(const HitResult& hit) {
    if (!hit.hit || effects_.size() + 2 > maximumEffects) return;
    glm::vec3 normal = glm::length(hit.normal) > 0.000001F
        ? glm::normalize(hit.normal) : glm::vec3{0.0F, 1.0F, 0.0F};
    const glm::vec3 helper = std::abs(normal.y) < 0.95F
        ? glm::vec3{0.0F, 1.0F, 0.0F} : glm::vec3{1.0F, 0.0F, 0.0F};
    const glm::vec3 tangent = glm::normalize(glm::cross(normal, helper));
    const glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));
    const glm::vec3 color = hit.surface == SurfaceType::Flesh
        ? glm::vec3{1.0F, 0.08F, 0.05F}
        : (hit.surface == SurfaceType::Metal ? glm::vec3{0.5F, 0.8F, 1.0F}
                                            : glm::vec3{0.85F, 0.75F, 0.55F});
    const float lifetime = hit.surface == SurfaceType::Flesh ? 0.2F : 5.0F;
    const glm::vec3 center = hit.point + normal * 0.003F;
    effects_.push_back({center - tangent * 0.07F, center + tangent * 0.07F, color, lifetime});
    effects_.push_back({center - bitangent * 0.07F, center + bitangent * 0.07F, color, lifetime});
}

HitResult CombatSystem::trace(const glm::vec3& origin,const glm::vec3& direction,const float maximumDistance,const EntityId ignored) const {
    HitResult result;result.distance=maximumDistance;
    RayHit world;if(collision_!=nullptr&&collision_->raycast(origin,direction,maximumDistance,world)){result={true,0,world.distance,world.position,world.normal,SurfaceType::Stone};}
    RayHit dynamic;EntityId dynamicOwner=0;if(dynamicCollision_!=nullptr&&dynamicCollision_->raycast(origin,direction,result.distance,dynamic,ignored,&dynamicOwner)){result={true,dynamicOwner,dynamic.distance,dynamic.position,dynamic.normal,SurfaceType::Metal};}
    for(const EnemyActor& actor:enemies_){if(!actor.active||actor.state==EnemyState::Dead||actor.id==ignored)continue;float distance=0.0F;glm::vec3 normal{};if(rayBox(origin,direction,enemyBounds(actor),result.distance,distance,normal))result={true,actor.id,distance,origin+direction*distance,normal,SurfaceType::Flesh};}
    return result;
}

void CombatSystem::updateProjectiles(const float dt,const glm::vec3& playerPosition) {
    for(Projectile& p:projectiles_){if(!p.active)continue;p.remainingLifetime-=dt;if(p.remainingLifetime<=0.0F){p.active=false;continue;}p.previousPosition=p.position;const glm::vec3 displacement=p.velocity*dt;const float distance=glm::length(displacement);if(distance<=0.000001F)continue;const glm::vec3 direction=displacement/distance;
        HitResult hit;hit.distance=distance;Capsule sphere{p.position-glm::vec3{0,p.radius,0},p.radius*2.0F,p.radius};
        CollisionSweepHit staticHit;if(collision_&&collision_->sweepCapsule(sphere,displacement,staticHit))hit={true,0,distance*staticHit.fraction,staticHit.point,staticHit.normal,SurfaceType::Stone};
        CollisionSweepHit dynamicHit;EntityId owner=0;if(dynamicCollision_&&dynamicCollision_->sweepCapsule(sphere,displacement,dynamicHit,p.owner,&owner)&&distance*dynamicHit.fraction<hit.distance)hit={true,owner,distance*dynamicHit.fraction,dynamicHit.point,dynamicHit.normal,SurfaceType::Metal};
        for(const EnemyActor& actor:enemies_){if(!actor.active||actor.state==EnemyState::Dead||actor.id==p.owner)continue;AABB bounds=enemyBounds(actor);bounds.inflate(p.radius);float actorDistance=0;glm::vec3 normal{};if(rayBox(p.position,direction,bounds,hit.distance,actorDistance,normal))hit={true,actor.id,actorDistance,p.position+direction*actorDistance,normal,SurfaceType::Flesh};}
        if(hit.hit){p.position=hit.point;addImpact(hit);if(hit.entity!=0)queueDamage({p.id,p.instigator,hit.entity,p.damageType,p.directDamage,hit.point,hit.normal,p.velocity});explode(p,hit.point,playerPosition);p.active=false;}else p.position+=displacement;if(effects_.size()<maximumEffects)effects_.push_back({p.previousPosition,p.position,{0.2F,0.8F,1.0F},0.07F});}
    std::erase_if(projectiles_,[](const Projectile&p){return !p.active;});
}

void CombatSystem::explode(const Projectile& p,const glm::vec3& point,const glm::vec3& playerPosition) {
    if(p.splashDamage<=0.0F||p.splashRadius<=0.0F)return;
    for(const EnemyActor& actor:enemies_){if(!actor.active||actor.state==EnemyState::Dead)continue;const glm::vec3 center=actor.position+glm::vec3{0,0.9F,0};const float distance=glm::length(center-point);const float damage=splashDamageAtDistance(p.splashDamage,p.splashRadius,distance);if(damage<=0.0F)continue;RayHit block;const glm::vec3 delta=center-point;const glm::vec3 direction=distance>0.001F?delta/distance:glm::vec3{0,1,0};if(distance>0.001F&&((collision_!=nullptr&&collision_->raycast(point,direction,distance-0.01F,block))||(dynamicCollision_!=nullptr&&dynamicCollision_->raycast(point,direction,distance-0.01F,block,p.owner))))continue;queueDamage({p.id,p.instigator,actor.id,DamageType::Explosive,damage,center,{},delta});}
    const glm::vec3 playerCenter=playerPosition+glm::vec3{0,0.9F,0};const float playerDistance=glm::length(playerCenter-point);const float playerDamage=p.selfDamage?splashDamageAtDistance(p.splashDamage,p.splashRadius,playerDistance):0.0F;if(playerDamage>0.0F){RayHit block;const glm::vec3 delta=playerCenter-point;if(collision_==nullptr||playerDistance<=0.001F||!collision_->raycast(point,delta/playerDistance,playerDistance-0.01F,block))queueDamage({p.id,p.instigator,playerEntityId,DamageType::Explosive,playerDamage,playerCenter,{},delta});}
    if(effects_.size()<maximumEffects){effects_.push_back({point-glm::vec3{p.splashRadius,0,0},point+glm::vec3{p.splashRadius,0,0},{1,0.35F,0.05F},0.25F});effects_.push_back({point-glm::vec3{0,p.splashRadius,0},point+glm::vec3{0,p.splashRadius,0},{1,0.35F,0.05F},0.25F});}
}

void CombatSystem::updateEnemies(const float dt,const glm::vec3& playerPosition,const Capsule&) {
    if(gameplay_==nullptr)return;
    const glm::vec3 playerEye=playerPosition+glm::vec3{0,1.6F,0};
    for(EnemyActor& actor:enemies_){
        GameplayEntity* authored=gameplay_->find(actor.id);
        if(authored!=nullptr&&actor.state!=EnemyState::Dead)actor.active=authored->active;
        if(!actor.active||actor.state==EnemyState::Dead){synchronizeEnemy(actor);continue;}
        const EnemyDefinition* def=enemyDefinition(actor.definitionId);if(def==nullptr)continue;
        actor.attackCooldown=std::max(0.0F,actor.attackCooldown-dt);actor.stateTimer=std::max(0.0F,actor.stateTimer-dt);
        const glm::vec3 eye=actor.position+glm::vec3{0,def->height*0.8F,0};const glm::vec3 toPlayer=playerEye-eye;const float distance=glm::length(toPlayer);
        bool visible=gameplay_->vitals().alive&&distance<=def->sightDistance&&insidePerceptionCone(actor.forward,toPlayer,def->fieldOfViewDegrees);
        if(visible&&distance>0.001F){RayHit block;const glm::vec3 sight=toPlayer/distance;visible=collision_==nullptr||!collision_->raycast(eye,sight,distance-0.1F,block);if(visible&&dynamicCollision_!=nullptr)visible=!dynamicCollision_->raycast(eye,sight,distance-0.1F,block,actor.id);}
        if(actor.state==EnemyState::Pain){if(actor.stateTimer<=0.0F)actor.state=EnemyState::Chasing;synchronizeEnemy(actor);continue;}
        if(visible){actor.lastSeenPlayer=playerPosition;actor.lostSightTimer=def->lostSightDuration;if(actor.state==EnemyState::Idle){actor.state=EnemyState::Alert;actor.stateTimer=def->reactionTime;}else if(actor.state==EnemyState::Alert&&actor.stateTimer<=0.0F)actor.state=EnemyState::Chasing;}
        else {actor.lostSightTimer=std::max(0.0F,actor.lostSightTimer-dt);if(actor.lostSightTimer<=0.0F&&(actor.state==EnemyState::Chasing||actor.state==EnemyState::Attacking))actor.state=EnemyState::Idle;}
        if(actor.state==EnemyState::Chasing){if(visible&&distance<=def->attackRange)actor.state=EnemyState::Attacking;else moveEnemy(actor,*def,dt,actor.lastSeenPlayer);}
        if(actor.state==EnemyState::Attacking){if(!visible||distance>def->attackRange){actor.state=EnemyState::Chasing;}else if(actor.attackCooldown<=0.0F){actor.attackCooldown=1.0F/def->shotsPerSecond;const glm::vec3 direction=rng_.spreadDirection(toPlayer/distance,def->attackSpreadDegrees);RayHit block;bool worldBlocked=collision_!=nullptr&&collision_->raycast(eye,direction,distance,block);if(!worldBlocked&&dynamicCollision_!=nullptr)worldBlocked=dynamicCollision_->raycast(eye,direction,distance,block,actor.id);if(!worldBlocked){const glm::vec3 closest=eye+direction*glm::clamp(glm::dot(playerEye-eye,direction),0.0F,distance);if(glm::length(closest-playerEye)<=0.45F)queueDamage({actor.id,actor.id,playerEntityId,DamageType::Bullet,def->attackDamage,eye,-direction,direction*def->attackDamage});}if(effects_.size()<maximumEffects)effects_.push_back({eye,worldBlocked?block.position:eye+direction*distance,{1,0.2F,0.1F},0.1F});}}
        const glm::vec3 flat{toPlayer.x,0,toPlayer.z};if(glm::length(flat)>0.001F)actor.forward=glm::normalize(flat);synchronizeEnemy(actor);
    }
}

void CombatSystem::moveEnemy(EnemyActor& actor,const EnemyDefinition& def,const float dt,const glm::vec3& target) {
    glm::vec3 delta=target-actor.position;delta.y=0;if(glm::length(delta)<0.1F)return;glm::vec3 remaining=glm::normalize(delta)*def.movementSpeed*dt;Capsule shape{actor.position,def.height,def.radius};
    for(int iteration=0;iteration<3&&glm::length(remaining)>0.0001F;++iteration){CollisionSweepHit staticHit,dynamicHit;bool hasStatic=collision_&&collision_->sweepCapsule(shape,remaining,staticHit);bool hasDynamic=dynamicCollision_&&dynamicCollision_->sweepCapsule(shape,remaining,dynamicHit,actor.id);CollisionSweepHit hit;if(!hasStatic&&!hasDynamic){actor.position+=remaining;break;}hit=hasStatic&&(!hasDynamic||staticHit.fraction<dynamicHit.fraction)?staticHit:dynamicHit;const float fraction=std::max(0.0F,hit.fraction-0.001F);actor.position+=remaining*fraction;shape.position=actor.position;remaining*=1.0F-fraction;const float into=glm::dot(remaining,hit.normal);if(into<0.0F)remaining-=hit.normal*into;}
}

void CombatSystem::queueDamage(DamageEvent event){if(event.amount<=0.0F||!std::isfinite(event.amount))return;event.sequence=nextDamageSequence_++;damageQueue_.push_back(std::move(event));}
void CombatSystem::processDamage(){std::size_t processed=0;while(!damageQueue_.empty()&&processed++<maximumDamagePerTick){DamageEvent event=damageQueue_.front();damageQueue_.pop_front();if(event.target==playerEntityId){if(gameplay_==nullptr||!gameplay_->vitals().alive)continue;const int amount=std::max(0,static_cast<int>(std::lround(event.amount)));static_cast<void>(gameplay_->vitals().applyDamage(amount,event.type,event.source));damageIndicatorTimer_=0.4F;lastDamageDirection_=event.hitPoint; if(!gameplay_->vitals().alive)gameplay_->showMessage("You died",4.0F,10);continue;}EnemyActor* actor=enemy(event.target);if(actor==nullptr||!actor->active||actor->state==EnemyState::Dead)continue;actor->health=std::max(0,actor->health-static_cast<int>(std::lround(event.amount)));if(actor->health==0){actor->state=EnemyState::Dead;actor->active=false;if(GameplayEntity* authored=gameplay_?gameplay_->find(actor->id):nullptr)authored->active=false;killMarkerTimer_=0.28F;if(dynamicCollision_)dynamicCollision_->upsert(actor->id,enemyBounds(*actor),false);for(std::size_t index:actor->primitiveIndices)if(scene_&&index<scene_->primitives.size())scene_->primitives[index].visible=false;}else{const EnemyDefinition* def=enemyDefinition(actor->definitionId);actor->state=EnemyState::Pain;actor->stateTimer=def?def->painDuration:0.2F;}hitMarkerTimer_=0.14F;}}

void CombatSystem::updateCombatPickups(const Capsule& playerCapsule){if(gameplay_==nullptr)return;for(GameplayEntity& entity:gameplay_->mutableEntities()){if(entity.authored.type!=GameplayEntityType::Pickup||entity.collected||!entity.active||(entity.authored.pickupType!="weapon"&&entity.authored.pickupType!="ammo"))continue;const glm::vec3 center{entity.authored.authoredWorldTransform*glm::vec4{entity.authored.boxOffset,1}};const AABB bounds{center-entity.authored.boxSize*0.5F,center+entity.authored.boxSize*0.5F};if(!capsuleOverlapsAabb(playerCapsule,bounds))continue;bool collected=false;if(entity.authored.pickupType=="weapon"){const WeaponDefinition* def=weaponDefinition(entity.authored.itemId);if(def){const bool newlyOwned=grantWeapon(def->id,entity.authored.amount);collected=newlyOwned||entity.authored.amount>0;}}else collected=addAmmo(entity.authored.itemId,entity.authored.amount)>0;if(collected){entity.collected=true;entity.active=false;for(std::size_t index:entity.authored.primitiveIndices)if(scene_&&index<scene_->primitives.size())scene_->primitives[index].visible=false;gameplay_->showMessage("Picked up "+entity.authored.displayName,2.5F,2);}}}

AABB CombatSystem::enemyBounds(const EnemyActor& actor) const {const EnemyDefinition* def=enemyDefinition(actor.definitionId);const float radius=def?def->radius:0.4F,height=def?def->height:1.8F;return AABB{actor.position+glm::vec3{-radius,0,-radius},actor.position+glm::vec3{radius,height,radius}};}
void CombatSystem::synchronizeEnemy(EnemyActor& actor){if(scene_){for(std::size_t index:actor.primitiveIndices)if(index<scene_->primitives.size()){const GameplayEntity* entity=gameplay_?gameplay_->find(actor.id):nullptr;if(entity)scene_->primitives[index].worldTransform=glm::translate(glm::mat4{1},actor.position-actor.spawnPosition)*entity->authored.authoredWorldTransform;scene_->primitives[index].visible=actor.active&&actor.state!=EnemyState::Dead;}}if(dynamicCollision_)dynamicCollision_->upsert(actor.id,enemyBounds(actor),actor.active&&actor.state!=EnemyState::Dead);}

void CombatSystem::updatePresentation(const float dt) noexcept {hitMarkerTimer_=std::max(0.0F,hitMarkerTimer_-dt);killMarkerTimer_=std::max(0.0F,killMarkerTimer_-dt);muzzleFlashTimer_=std::max(0.0F,muzzleFlashTimer_-dt);damageIndicatorTimer_=std::max(0.0F,damageIndicatorTimer_-dt);for(auto&effect:effects_)effect.lifetime-=dt;std::erase_if(effects_,[](const CombatLineEffect&e){return e.lifetime<=0.0F;});}
const WeaponDefinition* CombatSystem::equippedDefinition() const noexcept {const WeaponInstance*w=equippedWeapon();return w?weaponDefinition(w->definitionId):nullptr;}
const WeaponInstance* CombatSystem::equippedWeapon() const noexcept {return equippedIndex_>=0&&static_cast<std::size_t>(equippedIndex_)<weapons_.size()?&weapons_[static_cast<std::size_t>(equippedIndex_)]:nullptr;}
int CombatSystem::reserveAmmo() const {const WeaponDefinition*d=equippedDefinition();return d?ammunition_.get(d->ammoType):0;}
const AmmoInventory& CombatSystem::ammunition() const noexcept{return ammunition_;}const std::vector<EnemyActor>& CombatSystem::enemies()const noexcept{return enemies_;}const std::vector<Projectile>&CombatSystem::projectiles()const noexcept{return projectiles_;}const std::vector<CombatLineEffect>&CombatSystem::effects()const noexcept{return effects_;}const HitResult&CombatSystem::lastTrace()const noexcept{return lastTrace_;}bool CombatSystem::hitMarkerVisible()const noexcept{return hitMarkerTimer_>0;}bool CombatSystem::killMarkerVisible()const noexcept{return killMarkerTimer_>0;}float CombatSystem::muzzleFlashRemaining()const noexcept{return muzzleFlashTimer_;}float CombatSystem::damageIndicatorRemaining()const noexcept{return damageIndicatorTimer_;}glm::vec3 CombatSystem::lastDamageDirection()const noexcept{return lastDamageDirection_;}std::uint64_t CombatSystem::rngSeed()const noexcept{return rng_.seed();}std::uint64_t CombatSystem::rngSequence()const noexcept{return rng_.sequence();}
std::string CombatSystem::debugSummary()const{const WeaponInstance*w=equippedWeapon();std::ostringstream out;out<<"F7 COMBAT  WEAPON "<<(w?w->definitionId:"NONE")<<"  STATE "<<(w?weaponStateName(w->state):"NONE")<<"  AMMO "<<(w?w->magazine:0)<<'/'<<reserveAmmo()<<"  ENEMIES "<<std::count_if(enemies_.begin(),enemies_.end(),[](const EnemyActor&e){return e.active&&e.state!=EnemyState::Dead;})<<"  PROJECTILES "<<projectiles_.size()<<"  RNG "<<rng_.seed()<<':'<<rng_.sequence();return out.str();}

CombatSaveState CombatSystem::captureState()const{CombatSaveState state;for(const WeaponInstance&w:weapons_)state.weapons.push_back({w.definitionId,w.magazine});if(const WeaponInstance*w=equippedWeapon())state.equippedWeapon=w->definitionId;state.ammunition=ammunition_.values();for(const EnemyActor&e:enemies_)state.enemies.push_back({e.name,e.health,e.position,e.active});return state;}
bool CombatSystem::validateState(const CombatSaveState&state,std::string&error)const{std::unordered_map<std::string,bool>owned;for(const SavedWeaponState&w:state.weapons){const WeaponDefinition*d=weaponDefinition(w.id);if(!d||w.magazine<0||w.magazine>d->magazineSize||owned.contains(w.id)){error="invalid saved weapon '"+w.id+"'";return false;}owned[w.id]=true;}if(!state.equippedWeapon.empty()&&!owned.contains(state.equippedWeapon)){error="equipped weapon is not owned";return false;}AmmoInventory ammo=ammunition_;if(!ammo.restore(state.ammunition)){error="invalid saved ammunition";return false;}for(const SavedEnemyState&e:state.enemies){const GameplayEntity*entity=gameplay_?gameplay_->findByName(e.name):nullptr;if(!entity||entity->authored.type!=GameplayEntityType::EnemySpawn||e.health<0||!std::isfinite(e.position.x)||!std::isfinite(e.position.y)||!std::isfinite(e.position.z)){error="invalid saved enemy '"+e.name+"'";return false;}}return true;}
bool CombatSystem::restoreState(const CombatSaveState&state,std::string&error){if(!validateState(state,error))return false;weapons_.clear();equippedIndex_=-1;for(const SavedWeaponState&saved:state.weapons){WeaponInstance instance{saved.id,saved.magazine>0?WeaponState::Ready:WeaponState::Empty,saved.magazine,0,0};weapons_.push_back(instance);if(saved.id==state.equippedWeapon)equippedIndex_=static_cast<int>(weapons_.size()-1);}if(equippedIndex_<0&&!weapons_.empty())equippedIndex_=0;static_cast<void>(ammunition_.restore(state.ammunition));for(const SavedEnemyState&saved:state.enemies){GameplayEntity*entity=gameplay_?gameplay_->findByName(saved.name):nullptr;if(!entity)continue;EnemyActor*actor=enemy(entity->authored.id);if(!actor)continue;actor->health=saved.health;actor->position=saved.position;actor->active=saved.active&&saved.health>0;actor->state=actor->active?EnemyState::Idle:EnemyState::Dead;actor->stateTimer=actor->attackCooldown=0;synchronizeEnemy(*actor);}projectiles_.clear();damageQueue_.clear();effects_.clear();return true;}
