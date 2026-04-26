#!/bin/bash
# Funky Moose Test Runner

API="http://127.0.0.1:5050"
FILES=("tests/test_files/loud_master.wav" "tests/test_files/dynamic_track.wav" "tests/test_files/problematic_bass.wav")
GENRES=("Pop" "Pop" "Techno")

for i in "${!FILES[@]}"; do
    FILE="${FILES[$i]}"
    GENRE="${GENRES[$i]}"
    echo "--- Testing $FILE ($GENRE) ---"
    
    # Upload
    UPLOAD_RES=$(curl -s -X POST -F "audio=@$FILE" -F "genre=$GENRE" "$API/upload")
    REQ_ID=$(echo $UPLOAD_RES | python3 -c "import sys, json; print(json.load(sys.stdin).get('req_id', ''))")
    
    if [ -z "$REQ_ID" ]; then
        echo "Upload failed for $FILE: $UPLOAD_RES"
        continue
    fi
    echo "Uploaded: ID=$REQ_ID"
    
    # Full Track Analysis
    echo "Starting full-track analysis..."
    curl -s -X POST "$API/analyze_full_track/$REQ_ID?lang=de" > /dev/null
    
    # Wait a moment for processing (FFmpeg needs some time)
    sleep 5
    
    # Fetch Results
    RESULT=$(curl -s "$API/analysis/$REQ_ID")
    LUFS=$(echo $RESULT | python3 -c "import sys, json; d=json.load(sys.stdin); print(d.get('summary', {}).get('measured_lufs', 'N/A'))")
    SCORE=$(echo $RESULT | python3 -c "import sys, json; d=json.load(sys.stdin); print(d.get('summary', {}).get('overall_score', 'N/A'))")
    
    echo "Results: LUFS=$LUFS, Score=$SCORE"
done
