# SW.AuxTechnos.Required Dynamic Hide/Show Fix

## Issue Identified
The initial implementation of SW.AuxTechnos.Required only handled the initial cameo addition but didn't properly handle dynamic removal/addition when required technos were destroyed or built after the cameo had already appeared.

**Problem Behavior:**
1. ✅ Initial State: Required techno doesn't exist → Cameo hidden (working)
2. ✅ Techno Built: Required techno exists → Cameo appears (working)
3. ❌ Techno Destroyed: Required techno no longer exists → Cameo should disappear but instead just grayed out (bug)

## Root Cause Analysis
The issue was in the `SWSidebarClass::RecheckCameo()` method, which only removed cameos when `pSuper->IsPresent` was false (i.e., when the SuperWeapon itself was lost), but didn't check if cameos with `SW.AuxTechnos.Required=true` should be completely removed when their SW.AuxTechnos requirements were no longer met.

## Solution Implemented

### Enhanced RecheckCameo() Logic
Modified the `RecheckCameo()` method in `src/Ext/Sidebar/SWSidebar/SWSidebarClass.cpp` to include two key improvements:

#### 1. Dynamic Removal Logic
```cpp
for (const auto& button : column->Buttons)
{
    // Current logic: Remove if SuperWeapon is not present
    if (!HouseClass::CurrentPlayer->Supers[button->SuperIndex]->IsPresent)
    {
        removeButtons.push_back(button->SuperIndex);
        continue;
    }

    // NEW LOGIC: Remove if SW.AuxTechnos.Required=true and requirements not met
    const auto pSWType = SuperWeaponTypeClass::Array.GetItemOrDefault(button->SuperIndex);
    if (pSWType)
    {
        const auto pSWExt = SWTypeExt::ExtMap.Find(pSWType);
        if (pSWExt->SW_AuxTechnos_Required && !pSWExt->IsAvailable(HouseClass::CurrentPlayer))
        {
            removeButtons.push_back(button->SuperIndex);
        }
    }
}
```

#### 2. Dynamic Re-Addition Logic
```cpp
// Check for SuperWeapons that should now be added back
// This handles the case where required technos are built and cameos should reappear
for (const auto superIdx : ScenarioExt::Global()->SWSidebar_Indices)
{
    // Check if this SuperWeapon is already in the sidebar
    bool alreadyPresent = false;
    for (const auto& column : sidebar.Columns)
    {
        for (const auto& button : column->Buttons)
        {
            if (button->SuperIndex == superIdx)
            {
                alreadyPresent = true;
                break;
            }
        }
        if (alreadyPresent)
            break;
    }

    // If not present, try to add it (AddButton will check all requirements)
    if (!alreadyPresent)
    {
        sidebar.AddButton(superIdx);
    }
}
```

## Complete Dynamic Behavior

The fix now provides complete dynamic behavior for SW.AuxTechnos.Required:

### Scenario 1: Standard Behavior (SW.AuxTechnos.Required=false)
1. **Game Start**: Cameo visible, grayed out (no Construction Yard)
2. **Build Construction Yard**: Cameo becomes available (not grayed)
3. **Destroy Construction Yard**: Cameo becomes grayed out again
4. **Rebuild Construction Yard**: Cameo becomes available again

### Scenario 2: Required Behavior (SW.AuxTechnos.Required=true)
1. **Game Start**: Cameo completely hidden (no Construction Yard)
2. **Build Construction Yard**: Cameo appears and is available
3. **Destroy Construction Yard**: Cameo completely disappears
4. **Rebuild Construction Yard**: Cameo reappears and is available

## Technical Integration

### Leverages Existing Infrastructure
- **IsAvailable() Method**: Reuses existing SW.AuxTechnos checking logic
- **AddButton() Method**: Existing method already has all requirement checks
- **SWSidebar_Indices**: Uses existing tracking of which SuperWeapons should be in sidebar
- **RecheckCameo() Hook**: Integrates with existing tech tree update system

### Performance Considerations
- **Efficient Checking**: Only checks requirements when tech tree changes occur
- **Minimal Overhead**: Reuses existing data structures and methods
- **Smart Re-Addition**: Only attempts to add cameos that aren't already present

## Edge Cases Handled

### Multiple SuperWeapons with Same Requirements
- Each SuperWeapon is checked independently
- Multiple cameos can appear/disappear based on the same techno

### Mixed Required/Non-Required SuperWeapons
- SuperWeapons with `Required=false` continue to work as before (always visible, grayed when unavailable)
- SuperWeapons with `Required=true` get dynamic hide/show behavior
- Both can coexist without interference

### Rapid Building/Destruction
- System responds to each tech tree update
- No race conditions or state inconsistencies
- Proper cleanup when cameos are removed

### Integration with Other Systems
- Works alongside BattlePoints/CommanderPoints checking
- Compatible with existing SuperWeapon sidebar features
- Maintains proper sorting and positioning

## Testing Scenarios

### Basic Functionality Test
```ini
[GP1_Bombardment]
SW.AuxTechnos=GACNST
SW.AuxTechnos.Required=true
CommanderPoints.Amount=-1
```

**Expected Behavior:**
1. Start game → No GP1_Bombardment cameo visible
2. Build Construction Yard → GP1_Bombardment cameo appears
3. Destroy Construction Yard → GP1_Bombardment cameo disappears
4. Rebuild Construction Yard → GP1_Bombardment cameo reappears

### Multiple Requirements Test
```ini
[GP2_Bombardment]
SW.AuxTechnos=GACNST,GAWEAP
SW.AuxTechnos.Required=true
```

**Expected Behavior:**
1. Start game → No cameo visible
2. Build Construction Yard → Cameo appears (Construction Yard satisfies requirement)
3. Destroy Construction Yard → Cameo disappears
4. Build War Factory → Cameo appears (War Factory satisfies requirement)
5. Destroy War Factory → Cameo disappears

## Quality Assurance

### Build Verification
- ✅ Code compiles successfully with no errors or warnings
- ✅ No breaking changes to existing functionality
- ✅ Maintains backward compatibility

### Code Quality
- **Clean Integration**: Uses existing patterns and infrastructure
- **Efficient Implementation**: Minimal performance impact
- **Robust Logic**: Handles edge cases and error conditions
- **Maintainable Code**: Clear, well-commented implementation

## Conclusion

The dynamic hide/show fix completes the SW.AuxTechnos.Required implementation by providing true dynamic cameo visibility based on techno availability. SuperWeapons with `Required=true` now properly appear and disappear as their dependencies are built and destroyed, providing modders with complete control over SuperWeapon progression and UI presentation.

This creates a seamless "unlockable SuperWeapon" system where cameos are completely hidden until prerequisites are met, then function normally once available, and can be hidden again if prerequisites are lost.
