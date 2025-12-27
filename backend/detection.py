"""
YOLO Animal Detection using Ultralytics and OpenCV.

This module handles all ML-related operations.
Includes a mock mode for systems where OpenCV/YOLO is not available.
"""
import os
import random
from typing import Tuple, Optional, List, Dict

# Try to import ML libraries
try:
    import cv2
    import numpy as np
    from ultralytics import YOLO
    ML_AVAILABLE = True
except ImportError:
    ML_AVAILABLE = False
    print("⚠️ ML libraries not available - running in mock mode")

# YOLO model instance (loaded once)
_model = None

# Animals we're interested in detecting (COCO dataset classes)
ANIMAL_CLASSES = {
    14: 'bird',
    15: 'cat', 
    16: 'dog',
    17: 'horse',
    18: 'sheep',
    19: 'cow',
    20: 'elephant',
    21: 'bear',
    22: 'zebra',
    23: 'giraffe'
}

# All class names from COCO (for reference)
COCO_CLASSES = [
    'person', 'bicycle', 'car', 'motorcycle', 'airplane', 'bus', 'train', 'truck', 'boat',
    'traffic light', 'fire hydrant', 'stop sign', 'parking meter', 'bench', 'bird', 'cat',
    'dog', 'horse', 'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe', 'backpack',
    'umbrella', 'handbag', 'tie', 'suitcase', 'frisbee', 'skis', 'snowboard', 'sports ball',
    'kite', 'baseball bat', 'baseball glove', 'skateboard', 'surfboard', 'tennis racket',
    'bottle', 'wine glass', 'cup', 'fork', 'knife', 'spoon', 'bowl', 'banana', 'apple',
    'sandwich', 'orange', 'broccoli', 'carrot', 'hot dog', 'pizza', 'donut', 'cake', 'chair',
    'couch', 'potted plant', 'bed', 'dining table', 'toilet', 'tv', 'laptop', 'mouse',
    'remote', 'keyboard', 'cell phone', 'microwave', 'oven', 'toaster', 'sink', 'refrigerator',
    'book', 'clock', 'vase', 'scissors', 'teddy bear', 'hair drier', 'toothbrush'
]


def load_model():
    """
    Load YOLOv8 model. Model is cached for reuse.
    Uses YOLOv8n (nano) for faster inference.
    """
    global _model
    
    if not ML_AVAILABLE:
        print("⚠️ ML libraries not available - using mock detection")
        return None
        
    if _model is None:
        print("🔄 Loading YOLOv8 model...")
        _model = YOLO('yolov8n.pt')  # Downloads automatically if not present
        print("✅ YOLOv8 model loaded successfully")
    return _model


def mock_detect_animals(image_path: str) -> Tuple[List[Dict], Optional[str]]:
    """
    Mock detection for testing when ML libraries are not available.
    Returns random animal detections for testing purposes.
    """
    mock_animals = ['cat', 'dog', 'bird', 'cow', 'horse', 'sheep']
    
    # Generate 0-3 random mock detections
    num_detections = random.randint(0, 3)
    detections = []
    
    for i in range(num_detections):
        animal = random.choice(mock_animals)
        detections.append({
            'class_id': list(ANIMAL_CLASSES.keys())[list(ANIMAL_CLASSES.values()).index(animal)] if animal in ANIMAL_CLASSES.values() else 0,
            'class_name': animal,
            'confidence': round(random.uniform(0.5, 0.98), 4),
            'is_animal': True,
            'bbox': [100 * i, 100 * i, 200 + 100 * i, 200 + 100 * i]  # Mock bbox
        })
    
    return detections, None  # No annotated image in mock mode


def detect_animals(image_path: str, draw_boxes: bool = True) -> Tuple[List[Dict], Optional[str]]:
    """
    Run YOLO detection on an image and find animals.
    Falls back to mock detection if ML libraries are not available.
    
    Args:
        image_path: Path to the image file
        draw_boxes: Whether to draw bounding boxes on the image
        
    Returns:
        Tuple of (list of detections, path to annotated image or None)
    """
    if not ML_AVAILABLE:
        return mock_detect_animals(image_path)
    
    model = load_model()
    if model is None:
        return mock_detect_animals(image_path)
    
    image = cv2.imread(image_path)
    if image is None:
        raise ValueError(f"Could not load image from {image_path}")
    
    # Run YOLO inference
    results = model(image, verbose=False)
    
    detections = []
    annotated_image_path = None
    
    for result in results:
        boxes = result.boxes
        
        for box in boxes:
            class_id = int(box.cls[0])
            confidence = float(box.conf[0])
            class_name = COCO_CLASSES[class_id] if class_id < len(COCO_CLASSES) else 'unknown'
            
            detection = {
                'class_id': class_id,
                'class_name': class_name,
                'confidence': round(confidence, 4),
                'is_animal': class_id in ANIMAL_CLASSES,
                'bbox': box.xyxy[0].tolist()
            }
            detections.append(detection)
            
            # Draw bounding box if requested
            if draw_boxes:
                x1, y1, x2, y2 = map(int, box.xyxy[0])
                color = (0, 255, 0) if class_id in ANIMAL_CLASSES else (255, 165, 0)
                cv2.rectangle(image, (x1, y1), (x2, y2), color, 2)
                
                label = f"{class_name}: {confidence:.2f}"
                cv2.putText(image, label, (x1, y1 - 10), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
    
    # Save annotated image
    if draw_boxes and detections:
        base, ext = os.path.splitext(image_path)
        annotated_image_path = f"{base}_detected{ext}"
        cv2.imwrite(annotated_image_path, image)
    
    return detections, annotated_image_path


def get_primary_detection(detections: List[Dict]) -> Tuple[str, float]:
    """
    Get the primary (highest confidence) animal detection.
    
    Args:
        detections: List of detection dictionaries
        
    Returns:
        Tuple of (animal name, confidence score)
    """
    # Filter for animals first
    animal_detections = [d for d in detections if d['is_animal']]
    
    if animal_detections:
        # Get highest confidence animal
        best = max(animal_detections, key=lambda x: x['confidence'])
        return best['class_name'], best['confidence']
    elif detections:
        # If no animals, return highest confidence detection
        best = max(detections, key=lambda x: x['confidence'])
        return best['class_name'], best['confidence']
    else:
        return 'none', 0.0


def is_ml_available() -> bool:
    """Check if ML libraries are available."""
    return ML_AVAILABLE
