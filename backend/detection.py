"""
YOLO Animal Detection using ONNX Runtime.

This module handles all ML-related operations using ONNX Runtime
for lightweight, fast inference without PyTorch dependency.
Includes a mock mode for systems where ONNX Runtime is not available.
"""
import os
import random
from typing import Tuple, Optional, List, Dict

# Try to import ML libraries
try:
    import cv2
    import numpy as np
    import onnxruntime as ort
    ML_AVAILABLE = True
except ImportError:
    ML_AVAILABLE = False
    print("⚠️ ML libraries not available - running in mock mode")

# ONNX session instance (loaded once)
_session = None
_input_name = None

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

# Model input size (YOLOv8 default)
INPUT_SIZE = 640


def load_model():
    """
    Load YOLOv8 ONNX model. Model is cached for reuse.
    Uses YOLOv8n (nano) for faster inference.
    """
    global _session, _input_name
    
    if not ML_AVAILABLE:
        print("⚠️ ML libraries not available - using mock detection")
        return None
        
    if _session is None:
        print("🔄 Loading YOLOv8 ONNX model...")
        
        # Check for ONNX model file
        model_path = os.path.join(os.path.dirname(__file__), 'yolov8n.onnx')
        if not os.path.exists(model_path):
            # Try alternate location
            model_path = 'yolov8n.onnx'
        
        if not os.path.exists(model_path):
            print("❌ ONNX model not found! Please ensure yolov8n.onnx is in the backend directory.")
            return None
        
        # Create ONNX Runtime session
        _session = ort.InferenceSession(model_path, providers=['CPUExecutionProvider'])
        _input_name = _session.get_inputs()[0].name
        
        print("✅ YOLOv8 ONNX model loaded successfully")
    return _session


def preprocess_image(image: np.ndarray) -> Tuple[np.ndarray, float, Tuple[int, int]]:
    """
    Preprocess image for YOLOv8 ONNX inference.
    
    Args:
        image: BGR image from cv2.imread
        
    Returns:
        Tuple of (preprocessed tensor, scale ratio, padding offset)
    """
    original_height, original_width = image.shape[:2]
    
    # Calculate scale to fit in INPUT_SIZE while maintaining aspect ratio
    scale = min(INPUT_SIZE / original_width, INPUT_SIZE / original_height)
    new_width = int(original_width * scale)
    new_height = int(original_height * scale)
    
    # Resize image
    resized = cv2.resize(image, (new_width, new_height))
    
    # Create padded image (letterbox)
    padded = np.full((INPUT_SIZE, INPUT_SIZE, 3), 114, dtype=np.uint8)
    pad_x = (INPUT_SIZE - new_width) // 2
    pad_y = (INPUT_SIZE - new_height) // 2
    padded[pad_y:pad_y + new_height, pad_x:pad_x + new_width] = resized
    
    # Convert BGR to RGB
    rgb = cv2.cvtColor(padded, cv2.COLOR_BGR2RGB)
    
    # Normalize to [0, 1] and transpose to CHW format
    tensor = rgb.astype(np.float32) / 255.0
    tensor = np.transpose(tensor, (2, 0, 1))
    tensor = np.expand_dims(tensor, axis=0)  # Add batch dimension
    
    return tensor, scale, (pad_x, pad_y)


def postprocess_output(output: np.ndarray, scale: float, padding: Tuple[int, int], 
                       original_size: Tuple[int, int], conf_threshold: float = 0.25) -> List[Dict]:
    """
    Postprocess YOLOv8 ONNX output to get detections.
    
    YOLOv8 ONNX output shape: (1, 84, 8400) where 84 = 4 (bbox) + 80 (classes)
    
    Args:
        output: Raw ONNX output
        scale: Scale ratio used in preprocessing
        padding: Padding offset (x, y)
        original_size: Original image size (width, height)
        conf_threshold: Confidence threshold for filtering
        
    Returns:
        List of detection dictionaries
    """
    # YOLOv8 output is (1, 84, 8400), transpose to (8400, 84)
    predictions = output[0].T
    
    detections = []
    pad_x, pad_y = padding
    orig_w, orig_h = original_size
    
    for pred in predictions:
        # First 4 values are bbox (cx, cy, w, h)
        cx, cy, w, h = pred[:4]
        # Rest are class scores
        class_scores = pred[4:]
        
        # Get best class
        class_id = int(np.argmax(class_scores))
        confidence = float(class_scores[class_id])
        
        if confidence < conf_threshold:
            continue
        
        # Convert from center format to corner format
        x1 = cx - w / 2
        y1 = cy - h / 2
        x2 = cx + w / 2
        y2 = cy + h / 2
        
        # Remove padding and scale back to original image
        x1 = (x1 - pad_x) / scale
        y1 = (y1 - pad_y) / scale
        x2 = (x2 - pad_x) / scale
        y2 = (y2 - pad_y) / scale
        
        # Clip to image bounds
        x1 = max(0, min(x1, orig_w))
        y1 = max(0, min(y1, orig_h))
        x2 = max(0, min(x2, orig_w))
        y2 = max(0, min(y2, orig_h))
        
        class_name = COCO_CLASSES[class_id] if class_id < len(COCO_CLASSES) else 'unknown'
        
        detections.append({
            'class_id': class_id,
            'class_name': class_name,
            'confidence': round(confidence, 4),
            'is_animal': class_id in ANIMAL_CLASSES,
            'bbox': [x1, y1, x2, y2]
        })
    
    # Apply NMS (Non-Maximum Suppression)
    detections = apply_nms(detections, iou_threshold=0.45)
    
    return detections


def apply_nms(detections: List[Dict], iou_threshold: float = 0.45) -> List[Dict]:
    """
    Apply Non-Maximum Suppression to filter overlapping boxes.
    """
    if not detections:
        return []
    
    # Sort by confidence
    detections = sorted(detections, key=lambda x: x['confidence'], reverse=True)
    
    keep = []
    while detections:
        best = detections.pop(0)
        keep.append(best)
        
        remaining = []
        for det in detections:
            if compute_iou(best['bbox'], det['bbox']) < iou_threshold:
                remaining.append(det)
        detections = remaining
    
    return keep


def compute_iou(box1: List[float], box2: List[float]) -> float:
    """
    Compute Intersection over Union between two boxes.
    """
    x1 = max(box1[0], box2[0])
    y1 = max(box1[1], box2[1])
    x2 = min(box1[2], box2[2])
    y2 = min(box1[3], box2[3])
    
    intersection = max(0, x2 - x1) * max(0, y2 - y1)
    area1 = (box1[2] - box1[0]) * (box1[3] - box1[1])
    area2 = (box2[2] - box2[0]) * (box2[3] - box2[1])
    union = area1 + area2 - intersection
    
    return intersection / union if union > 0 else 0


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
    
    session = load_model()
    if session is None:
        return mock_detect_animals(image_path)
    
    image = cv2.imread(image_path)
    if image is None:
        raise ValueError(f"Could not load image from {image_path}")
    
    original_height, original_width = image.shape[:2]
    
    # Preprocess
    tensor, scale, padding = preprocess_image(image)
    
    # Run ONNX inference
    outputs = session.run(None, {_input_name: tensor})
    
    # Postprocess
    detections = postprocess_output(
        outputs[0], 
        scale, 
        padding, 
        (original_width, original_height)
    )
    
    annotated_image_path = None
    
    # Draw bounding boxes
    if draw_boxes and detections:
        for det in detections:
            x1, y1, x2, y2 = map(int, det['bbox'])
            color = (0, 255, 0) if det['is_animal'] else (255, 165, 0)
            cv2.rectangle(image, (x1, y1), (x2, y2), color, 2)
            
            label = f"{det['class_name']}: {det['confidence']:.2f}"
            cv2.putText(image, label, (x1, y1 - 10), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
        
        # Save annotated image
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
