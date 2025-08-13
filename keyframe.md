# Keyframe-Based Rendering System Implementation

## Overview
Implemented a 100-frame look-ahead keyframe system to replace direct movement computation with pre-computed keyframe interpolation.

## What Was Changed

### 1. Keyframe Buffer Structure
- **Size**: 100 frames × 131,072 entities × 32 bytes = ~400MB buffer
- **Layout**: Each keyframe stores 8 floats:
  - `position` (vec3): X, Y, Z coordinates
  - `rotation` (float): Rotation angle in radians
  - `color` (vec4): RGBA color values

### 2. Compute Pipeline Changes
- **New Shader**: `movement_keyframe.comp` - generates keyframe data
- **Descriptor Sets**: Separate descriptor layout for entity + keyframe buffer binding
- **Staggered Updates**: Only computes keyframes for `(entityID + frame) % 100 == 0` entities per frame

### 3. Vertex Shader Rewrite
- **Keyframe Reading**: Reads pre-computed position, rotation, color from keyframe buffer
- **Matrix Construction**: Builds transform matrix on-the-fly from keyframe data
- **No Interpolation**: Currently uses exact keyframe (alpha = 0.0) to avoid jitter

### 4. Renderer Integration
- **Disabled Legacy Compute**: Commented out regular `movement.comp` dispatch
- **Keyframe Initialization**: Computes all 100 keyframes for all entities on startup
- **Staggered Runtime**: Updates only 1% of keyframes per frame after initialization

## Critical Issues Identified

### 1. **Staggered Update Logic Flaw**
**Problem**: The staggered update `(entityID + frame) % 100 == 0` means:
- Entity 0 updates only on frames 0, 100, 200...
- Entity 1 updates only on frames 99, 199, 299...
- Most entities only get ONE keyframe update per 100 frames

**Result**: 99% of entities stuck with stale keyframes from initialization

### 2. **Frame Counter Mismatch**
**Problem**: The frame counter increments every frame, but keyframes are indexed by `frame % 100`:
- Frame 150 → keyframe index 50
- Frame 151 → keyframe index 51
- But staggered updates compute keyframes for a fixed future time

**Result**: Keyframes don't correspond to actual frame times being rendered

### 3. **Time Calculation Issues**
**Problem**: Keyframe compute uses `totalTime = accumulatedTime + 100*deltaTime`:
- This represents a fixed future time point
- But we need keyframes for different time points (frame 0 through frame 99)
- All keyframes are computed for the same future time

**Result**: All keyframes represent the same moment, no temporal progression

### 4. **Missing Interpolation**
**Problem**: Vertex shader reads exact keyframes without interpolation:
- Set `alpha = 0.0` to avoid jitter
- No smooth motion between keyframes
- Entities appear to "teleport" between keyframe positions

### 5. **Initialization Race Condition**
**Problem**: Keyframe initialization uses `initializeAllKeyframes()` which:
- Computes all 100 frames sequentially
- But staggered logic `isInitialization = (totalTime < 2.0)` may not work
- May not actually fill all keyframes before switching to staggered mode

## Fundamental Design Issues

### 1. **Wrong Keyframe Time Mapping**
Current approach computes keyframes like:
```
keyframe[0] = computeMovement(time = T + 100*dt)  // All same time!
keyframe[1] = computeMovement(time = T + 100*dt)  // All same time!
```

Should be:
```
keyframe[0] = computeMovement(time = T + 0*dt)
keyframe[1] = computeMovement(time = T + 1*dt)
keyframe[99] = computeMovement(time = T + 99*dt)
```

### 2. **Staggered Update Strategy**
Current: Update random 1% of entities' single keyframe per frame
Better: Update next-needed keyframe for all entities (e.g., frame+1 keyframe)

### 3. **Buffer Management**
Current: Massive 400MB buffer, mostly unused
Better: Smaller rolling buffer (e.g., 10 frames ahead)

## Recommended Fixes

### 1. **Fix Time Calculation**
Update keyframe compute to generate proper time sequence:
```glsl
float frameTime = pc.baseTime + float(pc.currentFrame) * pc.deltaTime;
```

### 2. **Fix Staggered Logic**
Instead of entity-based staggering, use frame-based:
- Each frame, compute keyframes for `(currentFrame + lookAhead) % 100`
- All entities get updated keyframes for the same future frame

### 3. **Add Interpolation**
Restore proper interpolation in vertex shader:
```glsl
float alpha = fract(totalTime * 60.0); // 60 FPS
vec3 pos = mix(keyframe[frame], keyframe[frame+1], alpha);
```

### 4. **Reduce Buffer Size**
Use smaller rolling buffer (10-20 frames) instead of 100 frames

## Performance Analysis
- **Memory Usage**: ~400MB for keyframe buffer (excessive)
- **Compute Load**: 1% per frame during runtime (good), 100% during init (expensive)
- **Bandwidth**: Should be better than original, but current bugs negate benefits
- **Expected Improvement**: Should achieve 50-90% reduction in compute load when fixed