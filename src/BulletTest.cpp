#include "BulletTest.h"
#include <algorithm>

// コンストラクタ - Bullet初期化
BulletTest::BulletTest() {
    // 衝突設定
    collisionConfiguration = new btDefaultCollisionConfiguration();
    
    // 衝突ディスパッチャ
    dispatcher = new btCollisionDispatcher(collisionConfiguration);
    
    // ブロードフェーズ
    overlappingPairCache = new btDbvtBroadphase();
    
    // 衝突ワールド
    collisionWorld = new btCollisionWorld(dispatcher, overlappingPairCache, collisionConfiguration);
}

// デストラクタ - リソース解放
BulletTest::~BulletTest() {
    // コリジョンオブジェクトの解放
    for (int i = 0; i < collisionObjects.size(); i++) {
        collisionWorld->removeCollisionObject(collisionObjects[i]);
        delete collisionObjects[i];
    }
    collisionObjects.clear();
    
    // 形状の解放
    for (int i = 0; i < collisionShapes.size(); i++) {
        delete collisionShapes[i];
    }
    collisionShapes.clear();
    
    // 物理ワールドの解放
    delete collisionWorld;
    delete overlappingPairCache;
    delete dispatcher;
    delete collisionConfiguration;
}

// 更新処理
void BulletTest::Update() {
    // 衝突検出の実行
    collisionWorld->performDiscreteCollisionDetection();
    
    // 衝突結果の処理
    int numManifolds = collisionWorld->getDispatcher()->getNumManifolds();
    
    for (int i = 0; i < numManifolds; i++) {
        // 衝突マニフォールド
        btPersistentManifold* contactManifold = collisionWorld->getDispatcher()->getManifoldByIndexInternal(i);
        const btCollisionObject* objA = contactManifold->getBody0();
        const btCollisionObject* objB = contactManifold->getBody1();
        
        // 接触点の確認
        int numContacts = contactManifold->getNumContacts();
        
        if (numContacts > 0 && collisionCallback) {
            // 最初の接触点の情報を使用
            btManifoldPoint& pt = contactManifold->getContactPoint(0);
            SimpleVector3 point = ToSimpleVector(pt.getPositionWorldOnB());
            
            // コールバック呼び出し
            collisionCallback(objA->getUserPointer(), objB->getUserPointer(), point);
        }
    }
}

// 球の追加
void* BulletTest::AddSphere(const SimpleVector3& center, float radius, void* userData) {
    // 球の形状作成
    btSphereShape* sphereShape = new btSphereShape(radius);
    collisionShapes.push_back(sphereShape);
    
    // コリジョンオブジェクト作成
    btCollisionObject* obj = new btCollisionObject();
    obj->setCollisionShape(sphereShape);
    
    // 位置設定
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(ToBtVector(center));
    obj->setWorldTransform(transform);
    
    // ユーザーデータ設定
    obj->setUserPointer(userData);
    
    // 衝突ワールドに追加
    collisionWorld->addCollisionObject(obj);
    collisionObjects.push_back(obj);
    
    return obj;
}

// ボックスの追加
void* BulletTest::AddBox(const SimpleVector3& halfExtents, const SimpleVector3& position, void* userData) {
    // ボックス形状作成
    btBoxShape* boxShape = new btBoxShape(ToBtVector(halfExtents));
    collisionShapes.push_back(boxShape);
    
    // コリジョンオブジェクト作成
    btCollisionObject* obj = new btCollisionObject();
    obj->setCollisionShape(boxShape);
    
    // 位置設定
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(ToBtVector(position));
    obj->setWorldTransform(transform);
    
    // ユーザーデータ設定
    obj->setUserPointer(userData);
    
    // 衝突ワールドに追加
    collisionWorld->addCollisionObject(obj);
    collisionObjects.push_back(obj);
    
    return obj;
}

// オブジェクトの位置設定
void BulletTest::SetPosition(void* object, const SimpleVector3& position) {
    btCollisionObject* obj = static_cast<btCollisionObject*>(object);
    if (obj) {
        btTransform transform = obj->getWorldTransform();
        transform.setOrigin(ToBtVector(position));
        obj->setWorldTransform(transform);
    }
}

// コールバック設定
void BulletTest::SetCollisionCallback(std::function<void(void*, void*, const SimpleVector3&)> callback) {
    collisionCallback = callback;
}

// SimpleVector3からbtVector3への変換
btVector3 BulletTest::ToBtVector(const SimpleVector3& v) {
    return btVector3(v.x, v.y, v.z);
}

// btVector3からSimpleVector3への変換
SimpleVector3 BulletTest::ToSimpleVector(const btVector3& v) {
    SimpleVector3 result;
    result.x = v.getX();
    result.y = v.getY();
    result.z = v.getZ();
    return result;
}
