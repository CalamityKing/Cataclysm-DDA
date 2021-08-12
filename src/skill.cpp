#include "skill.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <utility>

#include "cata_utility.h"
#include "debug.h"
#include "game_constants.h"
#include "item.h"
#include "json.h"
#include "options.h"
#include "recipe.h"
#include "rng.h"
#include "translations.h"

// TODO: a map, for Barry's sake make this a map.
std::vector<Skill> Skill::skills;
std::map<skill_id, Skill> Skill::contextual_skills;

std::vector<SkillDisplayType> SkillDisplayType::skillTypes;

static const Skill invalid_skill;
static const SkillDisplayType invalid_skill_type;

/** @relates string_id */
template<>
const Skill &string_id<Skill>::obj() const
{
    for( const Skill &skill : Skill::skills ) {
        if( skill.ident() == *this ) {
            return skill;
        }
    }

    const auto iter = Skill::contextual_skills.find( *this );
    if( iter != Skill::contextual_skills.end() ) {
        return iter->second;
    }

    return invalid_skill;
}

/** @relates string_id */
template<>
bool string_id<Skill>::is_valid() const
{
    return &obj() != &invalid_skill;
}

Skill::Skill() : Skill( skill_id::NULL_ID(), to_translation( "nothing" ),
                            to_translation( "The zen-most skill there is." ),
                            std::set<std::string> {}, skill_displayType_id::NULL_ID() )
{
}

Skill::Skill( const skill_id &ident, const translation &name, const translation &description,
              const std::set<std::string> &tags, skill_displayType_id display_type )
    : _ident( ident ), _name( name ), _description( description ), _tags( tags ),
      _display_type( display_type )
{
}

std::vector<const Skill *> Skill::get_skills_sorted_by(
    std::function<bool ( const Skill &, const Skill & )> pred )
{
    std::vector<const Skill *> result;
    result.reserve( skills.size() );

    for( const Skill &sk : skills ) {
        if( !sk.obsolete() ) {
            result.push_back( &sk );
        }
    }

    std::sort( begin( result ), end( result ), [&]( const Skill * lhs, const Skill * rhs ) {
        return pred( *lhs, *rhs );
    } );

    return result;
}

void Skill::reset()
{
    skills.clear();
    contextual_skills.clear();
}

void Skill::load_skill( const JsonObject &jsobj )
{
    // TEMPORARY until 0.G: Remove "ident" support
    skill_id ident = skill_id( jsobj.has_string( "ident" ) ? jsobj.get_string( "ident" ) :
                               jsobj.get_string( "id" ) );
    skills.erase( std::remove_if( begin( skills ), end( skills ), [&]( const Skill & s ) {
        return s._ident == ident;
    } ), end( skills ) );

    translation name;
    jsobj.read( "name", name );
    translation desc;
    jsobj.read( "description", desc );
    std::unordered_map<std::string, int> companion_skill_practice;
    for( JsonObject jo_csp : jsobj.get_array( "companion_skill_practice" ) ) {
        companion_skill_practice.emplace( jo_csp.get_string( "skill" ), jo_csp.get_int( "weight" ) );
    }
    time_info_t time_to_attack;
    if( jsobj.has_object( "time_to_attack" ) ) {
        JsonObject jso_tta = jsobj.get_object( "time_to_attack" );
        jso_tta.read( "min_time", time_to_attack.min_time );
        jso_tta.read( "base_time", time_to_attack.base_time );
        jso_tta.read( "time_reduction_per_level", time_to_attack.time_reduction_per_level );
    }
    skill_displayType_id display_type = skill_displayType_id( jsobj.get_string( "display_category" ) );
    Skill sk( ident, name, desc, jsobj.get_tags( "tags" ), display_type );

    sk._time_to_attack = time_to_attack;
    sk._companion_combat_rank_factor = jsobj.get_int( "companion_combat_rank_factor", 0 );
    sk._companion_survival_rank_factor = jsobj.get_int( "companion_survival_rank_factor", 0 );
    sk._companion_industry_rank_factor = jsobj.get_int( "companion_industry_rank_factor", 0 );
    sk._companion_skill_practice = companion_skill_practice;
    sk._obsolete = jsobj.get_bool( "obsolete", false );

    if( sk.is_contextual_skill() ) {
        contextual_skills[sk.ident()] = sk;
    } else {
        skills.push_back( sk );
    }
}

SkillDisplayType::SkillDisplayType() : SkillDisplayType( skill_displayType_id::NULL_ID(),
            to_translation( "invalid" ) )
{
}

SkillDisplayType::SkillDisplayType( const skill_displayType_id &ident,
                                    const translation &display_string )
    : _ident( ident ), _display_string( display_string )
{
}

void SkillDisplayType::load( const JsonObject &jsobj )
{
    // TEMPORARY until 0.G: Remove "ident" support
    skill_displayType_id ident = skill_displayType_id(
                                     jsobj.has_string( "ident" ) ? jsobj.get_string( "ident" ) :
                                     jsobj.get_string( "id" ) );
    skillTypes.erase( std::remove_if( begin( skillTypes ),
    end( skillTypes ), [&]( const SkillDisplayType & s ) {
        return s._ident == ident;
    } ), end( skillTypes ) );

    translation display_string;
    jsobj.read( "display_string", display_string );
    const SkillDisplayType sk( ident, display_string );
    skillTypes.push_back( sk );
}

const SkillDisplayType &SkillDisplayType::get_skill_type( const skill_displayType_id &id )
{
    for( auto &i : skillTypes ) {
        if( i._ident == id ) {
            return i;
        }
    }
    return invalid_skill_type;
}

skill_id Skill::from_legacy_int( const int legacy_id )
{
    static const std::array<skill_id, 28> legacy_skills = { {
            skill_id::NULL_ID(), skill_id( "dodge" ), skill_id( "melee" ), skill_id( "unarmed" ),
            skill_id( "bashing" ), skill_id( "cutting" ), skill_id( "stabbing" ), skill_id( "throw" ),
            skill_id( "gun" ), skill_id( "pistol" ), skill_id( "shotgun" ), skill_id( "smg" ),
            skill_id( "rifle" ), skill_id( "archery" ), skill_id( "launcher" ), skill_id( "mechanics" ),
            skill_id( "electronics" ), skill_id( "cooking" ), skill_id( "tailor" ), skill_id::NULL_ID(),
            skill_id( "firstaid" ), skill_id( "speech" ), skill_id( "computer" ),
            skill_id( "survival" ), skill_id( "traps" ), skill_id( "swimming" ), skill_id( "driving" ),
        }
    };
    if( static_cast<size_t>( legacy_id ) < legacy_skills.size() ) {
        return legacy_skills[legacy_id];
    }
    debugmsg( "legacy skill id %d is invalid", legacy_id );
    return skills.front().ident(); // return a non-null id because callers might not expect a null-id
}

skill_id Skill::random_skill()
{
    return random_entry_ref( skills ).ident();
}

// used for the pacifist trait
bool Skill::is_combat_skill() const
{
    static const std::string combat_skill( "combat_skill" );
    return _tags.count( combat_skill ) > 0;
}

bool Skill::is_contextual_skill() const
{
    static const std::string contextual_skill( "contextual_skill" );
    return _tags.count( contextual_skill ) > 0;
}

void SkillLevel::train( int amount, float catchup_modifier, float knowledge_modifier,
                        bool skip_scaling )
{
    // catchup gets faster the higher the level gap gets.
    float level_gap = std::max( _knowledgeLevel * 1.0f, 1.0f ) / std::max( _level * 1.0f, 1.0f );
    float catchup_amount = amount * catchup_modifier;
    float knowledge_amount = amount * knowledge_modifier;
    if( _knowledgeLevel > _level ) {
        catchup_amount *= level_gap;
    }
    if( _knowledgeLevel == _level && _knowledgeExperience > _exercise ) {
        // when you're in the same level, the catchup starts to slow down.
        catchup_amount = std::max( amount * ( catchup_modifier - ( exercise() * 1.0f / knowledgeExperience()
                                              *
                                              1.0f ) ),
                                   amount * 1.0f );
        knowledge_amount = std::max( amount * ( knowledge_modifier - 0.1f * ( exercise() * 1.0f /
                                                knowledgeExperience() * 1.0f ) ),
                                     amount * 1.0f );
    } else {
        // When your two xp's are equal just do the basic thing.
        catchup_amount = amount * 1.0f;
        knowledge_amount = amount * 1.0f;
    }

    // Learning knowledge faster than skill, when you're actually practicing, will generate some annoying problems.
    if( knowledge_amount > catchup_amount * 0.9f ) {
        knowledge_amount = catchup_amount * 0.9f;
    }

    if( _knowledgeLevel >= MAX_SKILL ) {
        knowledge_amount = 0;
    }

    if( skip_scaling ) {
        _exercise += catchup_amount;
        _rustAccumulator -= catchup_amount;
        _knowledgeExperience += knowledge_amount;
    } else {
        const double scaling = get_option<float>( "SKILL_TRAINING_SPEED" );
        if( scaling > 0.0 ) {
            _exercise += std::ceil( catchup_amount * scaling );
            _rustAccumulator -= std::ceil( catchup_amount * scaling );
            _knowledgeExperience += std::ceil( knowledge_amount * scaling );
        }
    }

    int xp_to_level = 100 * 100 * ( _level + 1 ) * ( _level + 1 );

    // Continue to level up while there is xp to do so
    while( _exercise >= xp_to_level ) {
        _exercise -= xp_to_level;
        ++_level;
        if( _level > _knowledgeLevel ) {
            _knowledgeLevel = _level;
            _knowledgeExperience = 0;
        }
        // Recalculate xp to level now that we have levelled up
        xp_to_level = 100 * 100 * ( _level + 1 ) * ( _level + 1 );
    }

    if( _rustAccumulator < 0 ) {
        _rustAccumulator = 0;
    }
    if( _level == _knowledgeLevel && _exercise > _knowledgeExperience ) {
        _knowledgeExperience = _exercise;
    }

    if( _knowledgeExperience >= 10000 * ( _knowledgeLevel + 1 ) * ( _knowledgeLevel + 1 ) ) {
        _knowledgeExperience = 0;
        ++_knowledgeLevel;
    }
}


void SkillLevel::knowledge_train( int amount, int npc_knowledge, bool skip_scaling )
{
    float level_gap = 1.0f;
    // when your _level is the same or 1 level below your knowledge, gain xp at the normal rate.
    // as your practical skill lags behind your knowledge, it gets harder to contextualize that
    // theoretical knowledge, and your ability to learn the theory gets slower.

    // The same formula applies to NPCs teaching you, but in that case the level decreases as their knowledge
    // level exceeds your own.  The best teacher is one who is only somewhat more knowledgeable than you.
    if( npc_knowledge > 0 ) {
        // This should later be modified by NPC teaching proficiencies.
        level_gap = std::max( npc_knowledge * 1.0f - _knowledgeLevel * 1.0f, 1.0f );
    } else {
        // Some day this should be affected by json specific to the skill, some skills are more amenable
        // to book learning.
        level_gap = std::max( _knowledgeLevel * 1.0f - _level * 1.0f, 1.0f );
    }
    float level_mult = 2.0f / ( level_gap + 1.0f );
    amount *= level_mult;

    if( skip_scaling ) {
        _knowledgeExperience += amount;
    } else {
        const double scaling = get_option<float>( "SKILL_TRAINING_SPEED" );
        if( scaling > 0.0 ) {
            _knowledgeExperience += std::ceil( amount * scaling );
        }
    }

    if( _knowledgeExperience >= 10000 * ( _knowledgeLevel + 1 ) * ( _knowledgeLevel + 1 ) ) {
        _knowledgeExperience = 0;
        ++_knowledgeLevel;
    }

}

bool SkillLevel::isRusting() const
{
    return _rustAccumulator > 0;
}

bool SkillLevel::rust( int rust_resist )
{
    if( _level >= MAX_SKILL ) {
        // don't rust any more once you hit the level cap, at least until we have a way to "pause" rust for a while.
        return false;
    }

    const int level_multiplier = ( _level + 1 ) * ( _level + 1 );
    float level_exp = level_multiplier * 10000.0f;
    if( _rustAccumulator > level_exp * 3 ) {
        // at this point the numbers ahead will be too small to bother.  Just cap it off.
        return false;
    }

    // Future plans: Have rust_slowdown impacted by intelligence and other memory-affecting things
    float rust_slowdown = std::max( static_cast<float>( std::sqrt( _rustAccumulator / level_exp ) ),
                                    0.04f );

    // rust amount starts at 4% of a level's xp, run every 24 hours.
    // Once the accumulated rust exceeds 16% of a level, rust_amount starts to drop.
    int rust_amount = level_multiplier * 16 / rust_slowdown;

    if( rust_resist > 0 ) {
        rust_amount = rust_amount * std::max( ( 100 - rust_resist ), 0 ) / 100;
    }

    if( _level == 0 ) {
        rust_amount = std::min( rust_amount, _exercise );
    }

    if( rust_amount < 1 ) {
        return false;
    }

    _rustAccumulator += rust_amount;
    _exercise -= rust_amount;
    const std::string &rust_type = get_option<std::string>( "SKILL_RUST" );
    if( _exercise < 0 ) {
        if( rust_type == "vanilla" || rust_type == "int" ) {
            _exercise = ( 100 * 100 * level_multiplier ) - 1;
            --_level;
        } else {
            _exercise = 0;
        }
    }

    return false;
}

void SkillLevel::practice()
{
    _lastPracticed = calendar::turn;
}

void SkillLevel::readBook( int minimumGain, int maximumGain, int maximumLevel )
{
    if( _knowledgeLevel < maximumLevel || maximumLevel < 0 ) {
        knowledge_train( ( _knowledgeLevel + 1 ) * rng( minimumGain, maximumGain ) * 100 );
    }

    practice();
}

bool SkillLevel::can_train() const
{
    return get_option<float>( "SKILL_TRAINING_SPEED" ) > 0.0;
}

const SkillLevel &SkillLevelMap::get_skill_level_object( const skill_id &ident ) const
{
    static const SkillLevel null_skill{};

    if( ident && ident->is_contextual_skill() ) {
        debugmsg( "Skill \"%s\" is context-dependent.  It cannot be assigned.", ident.str() );
        return null_skill;
    }

    const auto iter = find( ident );

    if( iter != end() ) {
        return iter->second;
    }

    return null_skill;
}

SkillLevel &SkillLevelMap::get_skill_level_object( const skill_id &ident )
{
    static SkillLevel null_skill;

    if( ident && ident->is_contextual_skill() ) {
        debugmsg( "Skill \"%s\" is context-dependent.  It cannot be assigned.", ident.str() );
        return null_skill;
    }

    return ( *this )[ident];
}

void SkillLevelMap::mod_skill_level( const skill_id &ident, int delta )
{
    SkillLevel &obj = get_skill_level_object( ident );
    obj.level( obj.level() + delta );
}

void SkillLevelMap::mod_knowledge_level( const skill_id &ident, int delta )
{
    SkillLevel &obj = get_skill_level_object( ident );
    obj.knowledgeLevel( obj.knowledgeLevel() + delta );
}

int SkillLevelMap::get_skill_level( const skill_id &ident ) const
{
    return get_skill_level_object( ident ).level();
}

int SkillLevelMap::get_skill_level( const skill_id &ident, const item &context ) const
{
    const auto id = context.is_null() ? ident : context.contextualize_skill( ident );
    return get_skill_level( id );
}

int SkillLevelMap::get_knowledge_level( const skill_id &ident ) const
{
    return get_skill_level_object( ident ).knowledgeLevel();
}

int SkillLevelMap::get_knowledge_level( const skill_id &ident, const item &context ) const
{
    const auto id = context.is_null() ? ident : context.contextualize_skill( ident );
    return get_knowledge_level( id );
}

bool SkillLevelMap::meets_skill_requirements( const std::map<skill_id, int> &req ) const
{
    return meets_skill_requirements( req, item() );
}

bool SkillLevelMap::meets_skill_requirements( const std::map<skill_id, int> &req,
        const item &context ) const
{
    return std::all_of( req.begin(), req.end(),
    [this, &context]( const std::pair<skill_id, int> &pr ) {
        // Whether or not you meet skill requirements should be based on your level of theory training,
        // not practical experience.
        return get_knowledge_level( pr.first, context ) >= pr.second;
    } );
}

std::map<skill_id, int> SkillLevelMap::compare_skill_requirements(
    const std::map<skill_id, int> &req ) const
{
    return compare_skill_requirements( req, item() );
}

std::map<skill_id, int> SkillLevelMap::compare_skill_requirements(
    const std::map<skill_id, int> &req, const item &context ) const
{
    std::map<skill_id, int> res;

    for( const auto &elem : req ) {
        const int diff = get_skill_level( elem.first, context ) - elem.second;
        if( diff != 0 ) {
            res[elem.first] = diff;
        }
    }

    return res;
}

int SkillLevelMap::exceeds_recipe_requirements( const recipe &rec ) const
{
    int over = rec.skill_used ? get_skill_level( rec.skill_used ) - rec.difficulty : 0;
    for( const auto &elem : compare_skill_requirements( rec.required_skills ) ) {
        over = std::min( over, elem.second );
    }
    return over;
}

bool SkillLevelMap::theoretical_recipe_requirements( const recipe &rec ) const
{
    // Regardless of your current practical skill, do you know the theory of how to make this thing?
    int knowhow = rec.skill_used ? get_knowledge_level( rec.skill_used ) - rec.difficulty : 0;
    for( const auto &elem : compare_skill_requirements( rec.required_skills ) ) {
        knowhow = std::min( knowhow, elem.second );
    }
    return ( knowhow > 0 );
}

bool SkillLevelMap::has_recipe_requirements( const recipe &rec ) const
{
    return ( exceeds_recipe_requirements( rec ) >= 0 || theoretical_recipe_requirements( rec ) );
}

// Actually take the difference in social skill between the two parties involved
// Caps at 200% when you are 5 levels ahead, int comparison is handled in npctalk.cpp
double price_adjustment( int barter_skill )
{
    if( barter_skill <= 0 ) {
        return 1.0;
    }
    if( barter_skill >= 5 ) {
        return 2.0;
    }
    switch( barter_skill ) {
        case 1:
            return 1.05;
        case 2:
            return 1.15;
        case 3:
            return 1.30;
        case 4:
            return 1.65;
        default:
            // Should never occur
            return 1.0;
    }
}
